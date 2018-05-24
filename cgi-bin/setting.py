import sys, os

sys.path.append('..') 
from util.command import show_help
command=[""]
help_str='Show lighttpd conf setting'

if show_help([help_str] + command):
    sys.exit()

filename="/usr/local/www/conf/lighttpd.conf"
content_type="text/plain"

#Print header type
print("Content-Length: " + str(os.path.getsize(filename)))
print("Content-Type: " + content_type)

#If you finish to write header, please add \r\n
print("")

#with open(filename,'rb') as f:
with open(filename,'r') as f:
    #read file binary
    data = f.read()

    #print stdout by binary mode
    #sys.stdout.buffer.write(data)
    sys.stdout.write(data)
f.close()
