set pagination off
set $_exitcode = -1
run --debug
if $_exitcode != -1 
    quit
end
backtrace
printf "Program exited with code: $_exitcode\n"
quit