"""Intraday shadow ledger at indicative trade prices, not broker execution.
Requires independently advancing timestamps on BOTH legs. A candidate must
persist for two observations, then fills on a third qualifying observation.
"""
import math


class PaperBook:
    def __init__(self, config):
        self.config = config
        self.accounts = {}
        self.events = []
        self.confirm = {}

    def process(self, snapshot, now):
        c = self.config
        for row in snapshot['pairs']:
            key = row['id']
            account = self.accounts.setdefault(key, {'cash': c['capital']/len(c['pairs']),
                'equity': c['capital']/len(c['pairs']), 'position': None, 'last_a': 0,
                'last_b': 0, 'status': 'Waiting', 'current': False, 'closed_trades': 0})
            account['current'] = False
            if not row['current']:
                account['status'] = 'Paused: stale, closed, or invalid data'
                self.confirm.pop(key, None)
                continue
            ta, tb = row['quote_time_a'], row['quote_time_b']
            if ta <= account['last_a'] or tb <= account['last_b']:
                account['status'] = 'Waiting for both legs to advance'
                continue
            account['last_a'], account['last_b'] = ta, tb
            pa, pb = row['price_a'], row['price_b']
            position = account['position']
            if position and position['day'] != row['quote_date']:
                account['status'] = 'Unresolved overnight position: paper account suspended'
                self.confirm.pop(key, None)
                continue
            account['current'] = True
            if position:
                # Calendar seconds, based on the last accepted marks; no interest on short proceeds.
                elapsed = max(0, now-position['mark_at'])
                short = max(0, -position['qa']*position['mark_a']) + max(0, -position['qb']*position['mark_b'])
                cost = (short*c['borrow_rate'] + max(0, short-account['cash'])*c['margin_rate'])*elapsed/(365*86400)
                account['cash'] -= cost
                position['holding_cost'] += cost
                account['equity'] = account['cash'] + position['qa']*pa + position['qb']*pb
                position.update(mark_at=now, mark_a=pa, mark_b=pb)
                z = row['z']
                reason = None
                if account['equity'] <= 0:
                    reason = 'insolvent'
                elif account['equity'] <= c['maintenance']*(abs(position['qa']*pa)+abs(position['qb']*pb)):
                    reason = 'capital_limit'
                elif account['equity']/position['entry_equity']-1 <= -c['pair_stop']:
                    reason = 'loss_stop'
                elif z is not None and abs(z) >= c['stop']:
                    reason = 'spread_stop'
                elif z is not None and position['direction']*z >= -c['exit']:
                    reason = 'convergence'
                elif now >= row['session_close']-300:
                    reason = 'session_end'
                elif now-position['opened_at'] >= 4*3600:
                    reason = 'four_hour_limit'
                if reason:
                    fees = 2*c['commission'] + (abs(position['qa']*pa)+abs(position['qb']*pb))*c['slippage_bps']/10000
                    account['cash'] = account['equity']-fees
                    account['equity'] = account['cash']
                    account['closed_trades'] += 1
                    account['blocked_day'] = row['quote_date']
                    account['position'] = None
                    account['status'] = 'Paper exit: '+reason
                    self.events.append({'type':'exit','pair':key,'at':now,'reason':reason,
                        'price_a':pa,'price_b':pb,'net_pnl':account['cash']-position['entry_equity'],
                        'entry_at':position['opened_at'],'execution_cost':fees,'holding_cost':position['holding_cost']})
                else:
                    account['status'] = 'Paper position open'
                continue
            account['equity'] = account['cash']
            if account['cash'] <= 0 or account.get('blocked_day') == row['quote_date']:
                account['status'] = 'No new entries this session'
                continue
            if not row['candidate'] or now >= row['session_close']-900:
                self.confirm.pop(key, None)
                account['status'] = 'No confirmed entry'
                continue
            signature = (row['direction'], row['train_end'])
            pending = self.confirm.get(key)
            streak = pending[1]+1 if pending and pending[0] == signature else 1
            self.confirm[key] = (signature, streak)
            account['status'] = 'Confirming candidate: '+str(streak)+'/3'
            if streak < 3:
                continue
            beta = row['model']['beta']
            dollars = c['gross']*account['equity']/(1+beta)
            direction = row['direction']
            qa, qb = direction*dollars/pa, -direction*beta*dollars/pb
            gross = abs(qa*pa)+abs(qb*pb)
            fees = 2*c['commission'] + gross*c['slippage_bps']/10000
            if account['equity']-fees <= c['maintenance']*gross:
                account['status'] = 'Entry rejected: capital limit'
                continue
            entry_equity = account['equity']
            account['cash'] -= qa*pa+qb*pb+fees
            account['equity'] -= fees
            account['position'] = {'qa':qa,'qb':qb,'direction':direction,'day':row['quote_date'],
                'opened_at':now,'entry_equity':entry_equity,'mark_at':now,'mark_a':pa,'mark_b':pb,'holding_cost':0}
            account['status'] = 'Paper entry'
            self.events.append({'type':'entry','pair':key,'at':now,'price_a':pa,'price_b':pb,
                'qa':qa,'qb':qb,'z':row['z'],'execution_cost':fees})
            self.confirm.pop(key, None)
        return {'accounts':self.accounts,'events':self.events[-200:],
                'equity':sum(a['equity'] for a in self.accounts.values()),
                'note':'Intraday shadow simulation only. Stale marks are retained and labelled. An unclosed overnight position is suspended.'}
