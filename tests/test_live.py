import copy,importlib.util,json,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from paper_book import PaperBook
from live_data import normalize
ROOT=Path(__file__).resolve().parents[1]
CONFIG=json.loads((ROOT/'config/default.json').read_text());CONFIG['pairs']=[['A','B']]
def row(t=1000):
    return {'id':'A / B','current':True,'candidate':True,'direction':1,'z':-2.5,
        'quote_time_a':t,'quote_time_b':t,'price_a':100.,'price_b':100.,
        'quote_date':'2026-10-02','train_end':'2026-10-01','session_close':30000.,'model':{'beta':1.}}
def process(book,r,t):return book.process({'pairs':[r]},t)
class PaperTests(unittest.TestCase):
    def setUp(self):self.book=PaperBook(CONFIG)
    def enter(self):
        for t in [1000,1001,1002]:process(self.book,row(t),t)
    def test_two_observations_do_not_fill(self):
        for t in [1000,1001]:process(self.book,row(t),t)
        self.assertEqual(len(self.book.events),0)
    def test_third_observation_fills(self):
        self.enter();self.assertEqual(self.book.events[0]['type'],'entry');self.assertLess(self.book.accounts['A / B']['equity'],CONFIG['capital'])
    def test_duplicate_timestamp_cannot_confirm(self):
        for _ in range(5):process(self.book,row(),1000)
        self.assertEqual(len(self.book.events),0)
    def test_one_leg_advance_cannot_confirm(self):
        for t in range(1000,1005):
            r=row(t);r['quote_time_b']=1000;process(self.book,r,t)
        self.assertEqual(len(self.book.events),0)
    def test_stale_breaks_confirmation(self):
        process(self.book,row(1000),1000);r=row(1001);r['current']=False;process(self.book,r,1001);process(self.book,row(1002),1002);process(self.book,row(1003),1003);self.assertEqual(len(self.book.events),0)
    def test_convergence_exit_books_costs(self):
        self.enter();r=row(1010);r.update(z=0,candidate=False,price_a=102);process(self.book,r,1010);self.assertEqual(self.book.events[-1]['reason'],'convergence');self.assertIsNone(self.book.accounts['A / B']['position']);self.assertAlmostEqual(self.book.events[-1]['net_pnl'],self.book.accounts['A / B']['cash']-CONFIG['capital'])
    def test_session_end_exit(self):
        self.enter();r=row(1020);r['session_close']=1200;process(self.book,r,1020);self.assertEqual(self.book.events[-1]['reason'],'session_end')
    def test_overnight_position_suspended(self):
        self.enter();r=row(2000);r['quote_date']='2026-10-05';process(self.book,r,2000);self.assertIn('suspended',self.book.accounts['A / B']['status']);self.assertEqual(len(self.book.events),1);self.assertFalse(self.book.accounts['A / B']['current'])
    def test_no_same_session_reentry_after_exit(self):
        self.enter();r=row(1010);r['z']=0;process(self.book,r,1010)
        for t in range(1020,1030):process(self.book,row(t),t)
        self.assertEqual(len(self.book.events),2)
    def test_invalid_quotes_do_not_mark_open_position(self):
        self.enter();old=self.book.accounts['A / B']['equity'];r=row(1100);r.update(current=False,price_a=999);process(self.book,r,1100);self.assertEqual(self.book.accounts['A / B']['equity'],old)
class AdapterTests(unittest.TestCase):
    def fixture(self):
        return {'meta':{'currency':'USD','dataGranularity':'1d','regularMarketPrice':105,'regularMarketTime':172800,'currentTradingPeriod':{'regular':{'start':172700,'end':180000}}},'timestamp':[86400,172800], 'indicators':{'quote':[{'close':[100,105]}]},'events':{}}
    def test_current_session_excluded(self):self.assertEqual(len(normalize(self.fixture(),172801)['history']),1)
    def test_missing_completed_close_rejected(self):
        r=self.fixture();r['indicators']['quote'][0]['close'][0]=None
        with self.assertRaises(ValueError):normalize(r,172801)
    def test_wrong_granularity_rejected(self):
        r=self.fixture();r['meta']['dataGranularity']='3mo'
        with self.assertRaises(ValueError):normalize(r,172801)
if __name__=='__main__':unittest.main()
