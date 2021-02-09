import sys
import subprocess
import pkg_resources

required = {'vosk', 'srt'}
installed = {pkg.key for pkg in pkg_resources.working_set}
missing = required - installed
print ("Missing pachages: ", missing)
if missing and len(sys.argv) > 1 :
    print ("Installing missing pachages: ", missing)
    python = sys.executable
    subprocess.check_call([python, '-m', 'pip', 'install', *missing], stdout=subprocess.DEVNULL)
