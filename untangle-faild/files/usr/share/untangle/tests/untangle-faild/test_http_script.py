import commands
import os

class TestHttpScript(object):
    @classmethod
    def setup_class( cls ):
        cls.prefix = "/"
        if ( "PREFIX" in os.environ ):
            cls.prefix = os.environ["PREFIX"]
        cls.script_path = os.path.join( cls.prefix, "usr", "share", "untangle-faild", "bin" )

        cls.http_script_path = os.path.join( cls.script_path, "http_test" )

    def test_valid_host( self ):
        yield self.check_is_valid, "http://localhost", 0

    def test_invalid_url( self ):
        yield self.check_is_valid, "http://localhost/asdf", 0

    def test_closed_port( self ):
        yield self.check_is_valid, "http://localhost:81", 256

    def test_unresolved_hostname( self ):
        yield self.check_is_valid, "http://fail.doesnotexist.nodomain/", 256

    def check_is_valid( self, url, is_valid ):
        status = commands.getstatusoutput( "FAILD_PRIMARY_ADDRESS=0.0.0.0 FAILD_TIMEOUT_MS=1 %s '%s'" % ( self.http_script_path, url ))
        assert status[0] == is_valid
(0, '')
    


