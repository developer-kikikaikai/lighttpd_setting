import sys

sys.path.append('..') 
from util.command import call_cmd, show_help, text_http_response
command=['ps','aux']
help_str='Running processes'

if show_help([help_str] + command):
    sys.exit()

result=call_cmd(command)
if result:
    text_http_response(result)
