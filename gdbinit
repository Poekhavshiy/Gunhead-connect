set pagination off
set $_exitcode = -1
run
if $_exitcode != -1 
    quit
end
backtrace
printf "Program exited with code: $_exitcode\n"
quit