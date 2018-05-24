import subprocess, sys

#call system command
def call_cmd(args):
    try:
        res_byte = subprocess.check_output(args)
        res=res_byte.decode('utf-8')
    except:
        print('Failed, command:', args )
        res=''
    return res

#show help
# @param ["help string", "command" (, "option") ]
# @return True if current option is "--help", and show help as "help string", "command" (, "option")
def show_help(help_data):
    args = sys.argv
    if 1 < len(args) and args[1] == "--help":
        print(",".join(help_data))
        return True
    else:
        return False

def text_http_response(result):
    print("Content-Length: " , len(result))
    print("Content-Type: text/plain")
    print("")
    print(result)
