
__all__ = ["cli", "rest"]

GIT_TAG = 'gracedb-1.10-1'

# issue 717.  Required for backward compatibility -- make sure "from ligo import gracedb"
# works as it used to.
from cli import *
