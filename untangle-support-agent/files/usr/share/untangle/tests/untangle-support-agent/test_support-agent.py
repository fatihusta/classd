import socket
from untangle.ats.utilities import is_process_running

class TestSupportAgent:

  SERVER = "supportsys.untangle.com"
  PORT = 6667
  TIMEOUT = 5 # in seconds
  
  def test_running(self):
    assert is_process_running("ruby /usr/bin/rbot")

  def test_connection(self):
    socket.setdefaulttimeout(self.TIMEOUT)
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((self.SERVER,self.PORT))
    assert True
