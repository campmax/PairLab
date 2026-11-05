#define PAIRLAB_LIVE_TEST
#include "../src/live.cpp"
#include <functional>
int main(int argc,char**argv){int passed=0,failed=0;auto test=[&](const std::string& name,const std::function<void()>& f){try{f();++passed;std::cout<<"PASS "<<name<<'\n';}catch(const std::exception& e){++failed;std::cout<<"FAIL "<<name<<": "<<e.what()<<'\n';}};
try{
 J fixture=readJson(argc>1?argv[1]:"tests/live_fixture.json");Config c;double now=fixture["assets"]["KO"]["quote_time"];
 auto first=[&](const J& j,double clock){return scoreLive(j,c,clock)["pairs"][0];};
 test("Fresh synthetic candidate",[&]{auto r=first(fixture,now);require(r["current"]&&r["candidate"],"Expected candidate");});
 test("Stale quotes block candidate",[&]{auto r=first(fixture,now+121);require(!r["current"]&&!r["candidate"],"Stale candidate");});
 test("Future quotes blocked",[&]{auto r=first(fixture,now-6);require(!r["current"],"Future quote");});
 test("Closed session blocked",[&]{auto j=fixture;for(auto symbol:{"KO","PEP"})j["assets"][symbol]["session_close"]=now-1;require(!first(j,now)["current"],"Closed signal");});
 test("Asynchronous pair quotes blocked",[&]{auto j=fixture;j["assets"]["PEP"]["quote_time"]=now-40;require(!first(j,now)["current"],"Skew signal");});
 test("Provider failure isolated to pair",[&]{auto j=fixture;j["assets"]["KO"]={{"error","Rate limited"}};auto r=scoreLive(j,c,now);require(!r["pairs"][0]["current"]&&r["pairs"][1]["current"],"Failure not isolated");});
 test("Invalid price rejected",[&]{auto j=fixture;j["assets"]["KO"]["price"]=-1;require(!first(j,now)["current"],"Bad price");});
 test("History gaps between legs rejected",[&]{auto j=fixture;j["assets"]["KO"]["history"].erase(j["assets"]["KO"]["history"].begin());require(!first(j,now)["current"],"Calendar mismatch");});
 test("Quote session excluded from training",[&]{auto j=fixture;j["assets"]["KO"]["history"].back()["date"]=j["assets"]["KO"]["quote_date"];require(!first(j,now)["current"],"Training leakage");});
 test("Training model unaffected by quote change",[&]{auto before=first(fixture,now);auto j=fixture;j["assets"]["KO"]["price"]=j["assets"]["KO"]["price"].get<double>()*1.01;auto after=first(j,now);require(before["model"]==after["model"],"Quote leaked into model");require(before["z"]!=after["z"],"Quote not scored");});
 test("Extreme deviation is not an entry",[&]{auto j=fixture;j["assets"]["KO"]["price"]=j["assets"]["KO"]["price"].get<double>()*2;auto r=first(j,now);require(r["current"]&&!r["candidate"],"Stop threshold bypass");});
 test("Unknown share basis rejected",[&]{auto j=fixture;j["assets"]["KO"]["basis"]="dividend_adjusted";require(!first(j,now)["current"],"Wrong price basis");});
 test("Synthetic label retained",[&]{require(scoreLive(fixture,c,now)["synthetic"],"Synthetic label lost");});
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
std::cout<<passed<<" passed; "<<failed<<" failed\n";return failed?1:0;}
