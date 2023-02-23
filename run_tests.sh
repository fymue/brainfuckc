#!/usr/bin/bash

BFC="./bf"

if [ ! -f $BFC ]; then
    make
fi

fail_c=0
success_c=0

check_equal() {
    real_answer=$2
    input=$3
    no_newline=$4
    bfc_answer="ans.txt"

    if [ -f "$1" ]; then
        # if input is file, call brainfuckc w/ file as input
        echo $input | $BFC $1 > $bfc_answer
    else
        # if not, parse arg as brainfuck string
        echo $input | $BFC --parse $1 > $bfc_answer
    fi
    
    # if real_answer isn't supposed to contain a trailing
    # newline, suppress the print of the trailing newline
    # (using $3's existence as an indicator for this)
    if [ "$no_newline" ]; then
        if [ -f "$2" ]; then
            DIFF=$(diff $bfc_answer $real_answer)
        else
            DIFF=$(echo -n $real_answer | diff $bfc_answer -)
        fi
    else
        if [ -f "$2" ]; then
            DIFF=$(diff $bfc_answer $real_answer)
        else
            DIFF=$(echo $real_answer | diff $bfc_answer -)
        fi
    fi

    if [ "$DIFF" ]; then
        echo --++--FAIL--++--
        echo $DIFF
        fail_c=$((fail_c+1))
    else
        echo OK
        success_c=$((success_c+1))
    fi

    rm -f ans.txt
}

check_equal test/helloworld.bf "Hello World!"
check_equal "+++[>--[>]----[----<]>---]>>.---.->..>++>-----.<<<<--.+>>>>>-[.<]" "hello world!" "" 1
check_equal test/h.bf "H"
check_equal test/self.bf "->++>+++>+>+>++>>+>+>+++>>+>+>++>+++>+++>+>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>+>+>++>>>+++>>>>>+++>+>>>>>>>>>>>>>>>>>>>>>>+++>>>>>>>++>+++>+++>+>>+++>+++>+>+++>+>+++>+>++>+++>>>+>+>+>+>++>+++>+>+>>+++>>>>>>>+>+>>>+>+>++>+++>+++>+>>+++>+++>+>+++>+>++>+++>++>>+>+>++>+++>+>+>>+++>>>+++>+>>>++>+++>+++>+>>+++>>>+++>+>+++>+>>+++>>+++>>+[[>>+[>]+>+[<]<-]>>[>]<+<+++[<]<<+]>>>[>]+++[++++++++++>++[-<++++++++++++++++>]<.<-<]" "" 1
check_equal test/squares.bf test/squares.ans
check_equal test/num_to_ascii_char.bf "a" "97" 1
check_equal test/num_to_ascii_char.bf "A" "65" 1
check_equal test/num_to_ascii_char.bf "9" "57" 1
check_equal test/num_to_ascii_char.bf "}" "125" 1

echo +==============================================================================+
printf "| Successfull: %6d |   Failed: %6d                                       |\n" $success_c $fail_c
echo +==============================================================================+

if [ $fail_c != 0 ]; then
  exit 1;
else
  exit 0;
fi