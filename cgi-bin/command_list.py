import sys, re

sys.path.append('..') 
from util.command import call_cmd, show_help, text_http_response
command=['ls']
help_str='Show all cgi command'

#call all command
result=call_cmd(['ls'])
if not result:
    sys.exit()

result=call_cmd(command)
if result:
    text_http_response(result)
