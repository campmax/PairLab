#include "statistics.hpp"
#include <chrono>
using namespace pl;

                                                                                
J scoreLive(const J& input, const Config& c, double now) {
    require(input.value("schema", "") == "pairlab.live-input.v1", "Unsupported live snapshot");
    require(std::isfinite(now), "Invalid clock");
    double ageLimit=input.value("max_age_seconds",120.0), skewLimit=input.value("max_skew_seconds",30.0);
    require(ageLimit>=1&&ageLimit<=300&&skewLimit>=0&&skewLimit<=120,"Invalid freshness limits");
    J rows=J::array();
    for (const auto& pair:c.pairs) {
        J row={{"id",pair.first+" / "+pair.second},{"a",pair.first},{"b",pair.second},
               {"current",false},{"candidate",false},{"z",nullptr},{"direction",0},{"issues",J::array()}};
        try {
            const auto& ja=input.at("assets").at(pair.first);
            const auto& jb=input.at("assets").at(pair.second);
            if(ja.contains("error"))throw std::runtime_error(pair.first+": "+ja.at("error").get<std::string>());
            if(jb.contains("error"))throw std::runtime_error(pair.second+": "+jb.at("error").get<std::string>());
            auto build=[&](const J& j,const std::string& symbol) {
                require(j.value("currency","")=="USD","Expected USD");
                require(j.value("basis","")=="current_split_units","Unsupported history share units");
                Asset a;a.symbol=symbol;
                for(const auto& v:j.at("history")) {
                    std::string date=v.at("date");double close=v.at("close"),div=v.at("dividend");
                    require(std::isfinite(close)&&close>0&&std::isfinite(div)&&div>=0,"Invalid history values");
                    int day=dayNumber(date);require(a.bars.empty()||day>a.bars.back().day,"Duplicate/unordered history");
                    double total=a.bars.empty()?0:a.logTotal.back()+std::log((close+div)/a.bars.back().close);
                    a.bars.push_back({date,day,close,close,close,close,div,1});a.logTotal.push_back(total);
                }
                require(a.bars.size()>=size_t(c.training)&&a.bars.size()<=3000,"Insufficient/excessive history");return a;
            };
            Asset a=build(ja,pair.first),b=build(jb,pair.second);
            require(a.bars.size()==b.bars.size(),"History calendars differ");
            for(size_t i=0;i<a.bars.size();++i)require(a.bars[i].date==b.bars[i].date,"History dates differ");
            auto model=fitModel(a,b,a.bars.size()-size_t(c.training),a.bars.size(),c);
            row["model"]=modelJson(model);row["train_end"]=a.bars.back().date;
            double pa=ja.at("price"),pb=jb.at("price"),ta=ja.at("quote_time"),tb=jb.at("quote_time");
            double da=ja.value("current_dividend",0.0),db=jb.value("current_dividend",0.0);
            require(std::isfinite(pa)&&std::isfinite(pb)&&pa>0&&pb>0&&std::isfinite(ta)&&std::isfinite(tb)&&ta>0&&tb>0,"Invalid quote");
            require(std::isfinite(da)&&std::isfinite(db)&&da>=0&&db>=0,"Invalid current distribution");
            std::string qa=ja.at("quote_date"),qb=jb.at("quote_date");
            require(qa==qb&&qa>a.bars.back().date,"Quotes must share a session after training");
            dayNumber(qa);
            row["price_a"]=pa;row["price_b"]=pb;row["quote_time_a"]=ta;row["quote_time_b"]=tb;row["quote_date"]=qa;
            row["age_a"]=now-ta;row["age_b"]=now-tb;
            double open=std::max(ja.at("session_open").get<double>(),jb.at("session_open").get<double>());
            double close=std::min(ja.at("session_close").get<double>(),jb.at("session_close").get<double>());
            require(std::isfinite(open)&&std::isfinite(close)&&open<close,"Missing regular-session schedule");
            row["session_open"]=open;row["session_close"]=close;
            bool current=true;
            auto block=[&](bool condition,const std::string& reason){if(condition){current=false;row["issues"].push_back(reason);}};
            block(ta>now+5||tb>now+5,"Future-dated quote");
            block(now-ta>ageLimit||now-tb>ageLimit,"Stale quote; no current signal");
            block(std::abs(ta-tb)>skewLimit,"Pair quotes are not synchronized");
            block(now<open||now>=close||ta<open||tb<open,"Outside current regular session");
            block(dayNumber(qa)-a.bars.back().day>5,"Training history is too old");
            if(model.sigma>0) {
                double y=a.logTotal.back()+std::log((pa+da)/a.bars.back().close);
                double x=b.logTotal.back()+std::log((pb+db)/b.bars.back().close);
                double z=zscore(model,y,x);require(std::isfinite(z),"Nonfinite live score");row["z"]=z;
                row["spread_gap_log"]=z*model.sigma;
                row["candidate"]=current&&model.eligible&&std::abs(z)>=c.entry&&std::abs(z)<c.stop;
                row["direction"]=z<0?1:-1;
            }
            row["current"]=current;
            if(!model.eligible)for(const auto& reason:model.reasons)row["issues"].push_back(reason);
        } catch(const std::exception& e) { row["issues"].push_back(e.what()); }
        rows.push_back(row);
    }
    return {{"schema","pairlab.live.v1"},{"generated_at",now},{"synthetic",input.value("synthetic",false)},
            {"source",input.value("source","")},{"max_age_seconds",ageLimit},{"entry_threshold",c.entry},{"stop_threshold",c.stop},{"pairs",rows},
            {"note","Intraday indicative last-trade marks against a daily historical model; not executable quotes or a probability of profit."}};
}
#ifndef PAIRLAB_LIVE_TEST
int main(int argc,char** argv) {
    try {
        require(argc==4,"Usage: pairlab_live input.json config.json output.json");
        double now=std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
        auto result=scoreLive(readJson(argv[1]),parseConfig(readJson(argv[2])),now);
        writeText(argv[3],result.dump());return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
#endif
