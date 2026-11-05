"""Local server integration test; pass path to the compiled live executable."""
import json,subprocess,sys,tempfile,time,urllib.request,urllib.error
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as directory:
    out=Path(directory)/'run'
    process=subprocess.Popen([sys.executable,str(ROOT/'tools/live_monitor.py'),'--exe',str(Path(sys.argv[1]).resolve()),'--demo','--paper','--interval','1','--cycles','4','--port','8772','--no-browser','--out',str(out)],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
    def request(path='/',host=None):
        return urllib.request.urlopen(urllib.request.Request('http://127.0.0.1:8772'+path,headers={'Host':host} if host else {}),timeout=2)
    try:
        for _ in range(100):
            try:response=request();break
            except OSError:time.sleep(.1)
        assert response.status==200 and response.headers['Cache-Control']=='no-store'
        assert b'PairLab' in response.read()
        for path,host,status in [('/config.json',None,404),('/', 'untrusted.example',403)]:
            try:request(path,host);raise AssertionError('Unexpected access')
            except urllib.error.HTTPError as e:assert e.code==status
        updates=set()
        while process.poll() is None:
            try:
                v=json.load(request('/api/latest'))
                if v['data']:updates.add(v['data']['generated_at'])
            except OSError:pass
            time.sleep(.15)
        output=process.communicate(timeout=5)[0];assert process.returncode==0,output;assert len(updates)>=2
        result=json.loads((out/'latest.json').read_text());assert result['status']=='stopped';assert result['data']['synthetic'];assert len(result['paper']['events'])>=1
        assert (out/'paper_events.jsonl').is_file();print('HTTP integration passed: live updates, Host guard, file isolation, paper persistence, stopped state.')
    finally:
        if process.poll() is None:process.terminate();process.wait(timeout=5)
