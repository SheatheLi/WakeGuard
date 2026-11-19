// Minimal explicit-state interleaving explorer for the park/submit race.
// It is an abstract model, not an executable proof about Linux futex/Folly.
#include <array>
#include <deque>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <cassert>
struct State {int phase=0;int queued=0;int permit=0;int sent=0;int consumed=0;};
// phase 0=worker preparing check, 1=checked empty before park (bug only),
// 2=parked, 3=processing, 4=finished.
using Key=std::array<int,5>;
Key key(State s){return {s.phase,s.queued,s.permit,s.sent,s.consumed};}
struct Node{State s;std::vector<std::string> trace;};
std::vector<std::pair<State,std::string>> edges(State s,bool correct){
 std::vector<std::pair<State,std::string>> e;
 if(!s.sent){State z=s;z.sent=1;z.queued=1;
   if(z.phase==2)z.permit=1; // notify if parked; an early buggy notify is lost
   e.push_back({z,"producer.submit"});}
 if(s.phase==0){
  State z=s;
  if(s.queued){z.phase=3;z.queued=0;e.push_back({z,"worker.consume_queued"});}
  else if(correct){z.phase=2;e.push_back({z,"worker.predicate_wait_atomic"});}
  else {z.phase=1;e.push_back({z,"worker.check_empty_without_lock"});}
 }
 if(s.phase==1){State z=s;z.phase=2;e.push_back({z,"worker.park_after_check"});}
 if(s.phase==2&&s.permit){State z=s;z.phase=3;z.permit=0;z.queued=0;e.push_back({z,"worker.wake_and_consume"});}
 if(s.phase==3){State z=s;z.phase=4;z.consumed=1;e.push_back({z,"worker.complete"});}
 return e;
}
struct Result{int reachable=0;bool deadlock=false;std::vector<std::string> counterexample;};
Result explore(bool correct){
 std::deque<Node> q;std::set<Key> visited;q.push_back({{}, {}});Result r;
 while(!q.empty()){
  Node n=q.front();q.pop_front();if(!visited.insert(key(n.s)).second)continue;r.reachable++;
  auto out=edges(n.s,correct);
  if(out.empty()&&n.s.sent&&!n.s.consumed){r.deadlock=true;r.counterexample=n.trace;return r;}
  for(auto [ns,event]:out){auto path=n.trace;path.push_back(event);q.push_back({ns,std::move(path)});}
 }
 return r;
}
int main(){auto broken=explore(false),correct=explore(true);
 assert(broken.deadlock && !correct.deadlock);
 std::cout<<"{\"broken_deadlock\":true,\"correct_deadlock\":false,\"counterexample\":[";
 for(size_t i=0;i<broken.counterexample.size();i++){if(i)std::cout<<",";std::cout<<"\""<<broken.counterexample[i]<<"\"";}
 std::cout<<"],\"bad_states\":"<<broken.reachable<<",\"correct_states\":"<<correct.reachable<<"}\n";
}
