from tools.linearizability import Op, linearizable, load

def test_accepted_overlap():
    x=[Op(0,'PUSH',4,1,4),Op(1,'POP',4,3,5)]
    assert linearizable(x)

def test_reject_fifo_violation():
    x=[Op(0,'PUSH',1,1,2),Op(1,'PUSH',2,3,4),Op(2,'POP',2,5,6),Op(3,'POP',1,7,8)]
    assert not linearizable(x)
