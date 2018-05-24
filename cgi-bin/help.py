import sys, re

def create_table(fname, help_list):
    result='<tr>\n'
    result+='  <td>' + fname + "</td>\n"
    result+='  <td>' + help_list[0] + "</td>\n"
    result+='  <td> '
    for i, cmd in enumerate(help_list):
        if i == 0:
            continue
        result+=cmd + ' '
    result+='  </td>\n'
    result+='</tr>\n'
    return result


sys.path.append('..') 
from util.command import call_cmd, show_help
command=['ls']
help_str='Show all command usage'

#show help
if show_help([help_str] + command):
    sys.exit()

#call all command
result=call_cmd(['ls'])
if not result:
    sys.exit()

#create help dict
help_dict={}
for fname in re.split(r'\n', result):
    if not fname:
        continue

    #if there is a --help, result if "string, command, option"
    result=call_cmd(['/usr/bin/python3.6', fname, '--help']).replace('\n', '').split(",")
    #is there a help?
    if 1 < len(result):
        help_dict[fname]=result

#show output
html='''<!DOCTYPE html>
<html>
  <head>
    <meta charset="utf-8" />
    </head>
    <style>
    table{
      border-collapse:collapse;
      margin:0 auto;
    }
    td,th{
      padding:10px;
      border-bottom:solid #ccc;
    }
    th{
      border-bottom:5px solid #ccc;
    }
    table tr th:nth-child(odd),
    table tr td:nth-child(odd){
      background:#e6f2ff;
    }
    </style>
    <body>
      <table>
        <thead>
        <tr>
         <th>cgi name</td>
         <th>usage</td>
         <th>use system command</td>
       </tr>
       </thead>
'''
for cgi, help_list in help_dict.items():
    html+=create_table(cgi, help_list)

html+='''
       </table>
    </body>
</html>'''

print("Content-Length: " , len(html))
print("Content-Type: text/html")
print("")
print(str(html))
