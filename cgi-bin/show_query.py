import sys, os

query_string=os.environ.get("QUERY_STRING")

sys.path.append('..') 
from util.command import show_help
command=[""]
help_str='Show query string'

if show_help([help_str] + command):
    sys.exit()

result=''
if not query_string:
    print('Empty query')
else:
    queries=query_string.split("&")
    for i, query in enumerate(queries):
        print(i,  '=>' , query)

