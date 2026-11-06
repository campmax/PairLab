pass                                                                            
import argparse, concurrent.futures, json, math, os, random, subprocess, sys, threading, time, webbrowser
from datetime import date, timedelta
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit
from live_data import fetch, iso_day
from paper_book import PaperBook

ROOT = Path(__file__).resolve().parents[1]


def atomic(path, value):
    temporary = path.with_suffix(path.suffix+'.tmp')
    temporary.write_text(json.dumps(value, allow_nan=False), encoding='utf-8')
    os.replace(temporary, path)


def append_journal(path, records):
                                                                            
    if path.exists() and path.stat().st_size > 20*1024*1024:
        os.replace(path, path.with_name(path.stem+'.previous.jsonl'))
    with path.open('a',encoding='utf-8') as stream:
        for record in records:
            stream.write(json.dumps(record,allow_nan=False)+'\n')


def demo_assets(config, step, now):
    assets = {}
    dates = []
    current = date.fromisoformat(iso_day(now))-timedelta(days=1)
    while len(dates) < max(650, config['training']+20):
        if current.weekday()<5:
            dates.append(current.isoformat())
        current -= timedelta(days=1)
    dates.reverse()
    for number, (sa, sb) in enumerate(config['pairs']):
        rng = random.Random(937+number); x=0; residual=0; a=[]; b=[]
        for day in dates:
            x += rng.gauss(0,.015); residual=.65*residual+rng.gauss(0,.015)
            a.append({'date':day,'close':100*math.exp(1.2*x+residual),'dividend':0})
            b.append({'date':day,'close':100*math.exp(x),'dividend':0})
        shock = .055 if step%10<5 else 0
        for symbol, history, price in [(sa,a,100*math.exp(1.2*x+shock)),(sb,b,100*math.exp(x))]:
            assets[symbol]={'currency':'USD','basis':'current_split_units','history':history,
                'price':price,'quote_time':now,'quote_date':iso_day(now),
                'session_open':now-3600,'session_close':now+6*3600,'current_dividend':0}
    return assets


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe',type=Path,default=ROOT/'bin/pairlab_live.exe')
    parser.add_argument('--config',type=Path,default=ROOT/'config/default.json')
    parser.add_argument('--interval',type=float,default=60)
    parser.add_argument('--port',type=int,default=8768)
    parser.add_argument('--out',type=Path)
    parser.add_argument('--cycles',type=int,default=0)
    parser.add_argument('--demo',action='store_true')
    parser.add_argument('--paper',action='store_true')
    parser.add_argument('--no-browser',action='store_true')
    args=parser.parse_args()
    if not args.exe.is_file():parser.error('Run build_windows.cmd first; live executable missing.')
    if args.interval<(1 if args.demo else 30) or not 1024<=args.port<=65535 or args.cycles<0:parser.error('Invalid interval, port or cycle count')
    config_text=args.config.read_text(encoding='utf-8');config=json.loads(config_text)
    symbols=sorted({s for pair in config['pairs'] for s in pair})
    if not 2<=len(symbols)<=8:parser.error('Expected 1–4 disjoint pairs')
    session=args.out.resolve() if args.out else ROOT/'runs'/('live_'+str(time.time_ns()))
    session.mkdir(parents=True,exist_ok=False)
    atomic(session/'config.json',config)
    book=PaperBook(config) if args.paper else None
    lock=threading.Lock();view={'status':'starting','message':'Collecting current data…','data':None,'paper':None}
    html=(ROOT/'web/live.html').read_bytes()
    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            if self.headers.get('Host') not in (f'127.0.0.1:{args.port}',f'localhost:{args.port}'):
                self.send_error(403);return
            path=urlsplit(self.path).path
            if path=='/':body=html;kind='text/html; charset=utf-8'
            elif path=='/api/latest':
                with lock:body=json.dumps(view,allow_nan=False).encode()
                kind='application/json'
            else:self.send_error(404);return
            self.send_response(200);self.send_header('Content-Type',kind);self.send_header('Cache-Control','no-store');self.send_header('X-Content-Type-Options','nosniff');self.send_header('Content-Length',str(len(body)));self.end_headers()
            try:self.wfile.write(body)
            except (BrokenPipeError,ConnectionResetError):pass
        def log_message(self,*args):pass
    try:server=ThreadingHTTPServer(('127.0.0.1',args.port),Handler)
    except OSError as e:parser.exit(1,f'Cannot open local dashboard: {e}. Stop the other monitor or use --port 8769.\n')
    threading.Thread(target=server.serve_forever,daemon=True).start()
    url=f'http://127.0.0.1:{args.port}/';print('Dashboard:',url,flush=True);print('Session:',session,flush=True)
    if not args.no_browser:webbrowser.open(url)
    def update(**values):
        with lock:view.update(values)
    step=0;failures=0;stop=threading.Event();logged_events=0
    try:
        while not args.cycles or step<args.cycles:
            update(status='collecting',message='Refreshing prices and completed-session history…')
            try:
                if args.config.read_text(encoding='utf-8')!=config_text:raise ValueError('Config changed. Restart for a new, separately logged session.')
                if args.demo:assets=demo_assets(config,step,time.time())
                else:
                    assets={}
                    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
                        jobs={pool.submit(fetch,s):s for s in symbols}
                        for future in concurrent.futures.as_completed(jobs):
                            symbol=jobs[future]
                            try:assets[symbol]=future.result()
                            except Exception as e:assets[symbol]={'error':str(e)}
                snapshot={'schema':'pairlab.live-input.v1','synthetic':args.demo,
                    'source':'Invented live demonstration' if args.demo else 'Yahoo public chart endpoint · indicative last trades; delay not guaranteed',
                    'max_age_seconds':120,'max_skew_seconds':30,'assets':assets}
                atomic(session/'input.json',snapshot)
                completed=subprocess.run([str(args.exe.resolve()),str(session/'input.json'),str(session/'config.json'),str(session/'scored.json')],capture_output=True,text=True,timeout=60)
                if completed.returncode:raise RuntimeError(completed.stderr[-1500:])
                result=json.loads((session/'scored.json').read_text())
                now=time.time();paper=book.process(result,now) if book else None
                if book:
                    atomic(session/'paper.json',paper)
                    append_journal(session/'paper_events.jsonl',book.events[logged_events:])
                    logged_events=len(book.events)
                append_journal(session/'signals.jsonl',[result])
                usable=any('price_a' in r for r in result['pairs'])
                status='ok' if usable else 'error'
                update(status=status,message='Watching; timestamps determine freshness.' if usable else 'No usable quotes. Inspect provider errors.',data=result,paper=paper,last_success=now)
                with lock:atomic(session/'latest.json',view)
                failures=0 if usable else failures+1
                print(f'Capture {step+1}: {sum(r["current"] for r in result["pairs"])} current pairs, {sum(r["candidate"] for r in result["pairs"])} candidates',flush=True)
            except Exception as e:
                failures+=1;update(status='error',message=str(e));print('Capture failed:',e,flush=True)
                if book:book.confirm.clear()
            step+=1
            if not args.cycles or step<args.cycles:stop.wait(min(300,args.interval*2**min(failures,3)))
    except KeyboardInterrupt:print('\nStopped. Audit files retained. Open paper positions are not silently closed.',flush=True)
    finally:
        update(status='stopped',message='Collector stopped. Open paper marks are not current.')
        with lock:atomic(session/'latest.json',view)
        server.shutdown();server.server_close()
if __name__=='__main__':main()
