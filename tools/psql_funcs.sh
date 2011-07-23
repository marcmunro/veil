#! /bin/sh
# psql_funcs.sh
#
#      Functions for use by psql shell scripts.
#
#      Copyright (c) 2005 - 2011 Marc Munro
#      Author:  Marc Munro
#      License: BSD
#

# psql_collate
# Filter test output from psql, cleaning and collating as we go.  Unfiltered
# output, and summary output goes to logfile $1.  Result is non-zero if
# the tests fail
#
psql_collate()
{
    tee -a $1 | awk '
        function finish_test()
        {
            if (!testing) return

            if ((condition == "=") ||
                (condition == "~"))
            {
                ok = ok && found
            }
            else {
                ok = ok && !found
            }

	    if (prepfail) {
	        failed++
		ok = 0
		res = "PREP FAIL\n  [" preplines "\n  ]"
	    }
            else {
                if (ok && lines != "") # If lines is empty, then no result
                {                      # from test, so consider it failed
                    passed++
                }
                else {
                    failed++
                }
		res = ok ? "PASS": "FAIL\n  [" lines "\n  ]"
            }

            printf("      %s\n", res)
            print res >>LOGFILE
            printf "END TEST %s\n\n", testno >>LOGFILE

            found = 0
            testing = 0
	    prepfail = 0
        }
        BEGIN {
            prepping = 0  # boolean
	    prepfail = 0  # boolean
            testing = 0   # boolean
            complete = 0  # boolean
            passed = 0    # counter
            failed = 0    # counter
            tests = 0     # counter
            printf("\n")
        }
        /^-/ {
            # Print lines starting with hyphen.  Such lines terminate any
            # open test
            finish_test()
            line = $0
            gsub(/^. ?/, "", line)
            print line
            next
        }
        /^COMPLETE/ {
            # The line identifies completion of all tests.  If this
            # line is not reached it may be because the pipe from psql
            # was broken.
            complete = 1
        }
        /^PREP/ {
	    # The line identifies the start of the preparation for a 
	    # test.  Lines between here and the TEST statement are
            # ignored unless an ERROR is detected which will cause the
	    # subsequent test to fail
            finish_test()
	    prepping = 1
	    prepfail = 0
            ignore = $2 == "IGNORE"
	    preplines = ""
	    next
	}
        /^TEST/ {
            # The line defines a test.  Format is:
            # TEST <testno> <condition> #<pattern to test using condition>
	    prepping = 0
            finish_test()
            testno = $2
            condition = $3
            nf = split($0, fields, /#/)
            pattern = fields[2]
            if (nf > 2) {
                summary = fields[3]
            }
            else {
                summary = "result " condition " " pattern
            }
            line = sprintf("%8s: %s", testno, summary)
            printf("%-65s", line)
            found = 0
            lines=""
            testing = 1
            ok = 1
            tests++
            print >>LOGFILE # All input lines go to logfile
                            # This is done explicitly so that the output
                            # from finish_test is logged first
            next            # No more to do for this line
        }
        { print >>LOGFILE }  # All input lines go to logfile
	(prepping) {
	    if ($0 ~ /ERROR/) {
                if (!ignore) {
	            prepfail = 1
                }
	    }
	    if (prepfail) {
                if (preplines == "") {
                    preplines = "   " $0
                }
                else {
	            preplines = preplines "\n   " $0
                }
	    }
	    next
	}
        {   # may be a result line.  Test against condition and pattern,
            # and accumlate the line for later output
            sub(/^[ 	]*/, "")  # Remove leading space
            sub(/[ 	]*$/, "") # Remove trailing space
#printf("\nComparing :%s:%s:%s:\n", $0, condition, pattern)
            if ((condition == "=") ||
                (condition == "!="))
            {
                if ($0 == pattern) {
#print "Found"
                    found = 1
		    printf "MATCH = \"%s\"\n", $0 >>LOGFILE
                }
            }
            else if ((condition == "~") ||
                     (condition == "!~")) 
            {
                if ($0 ~ pattern) {
                    found = 1
		    printf "MATCH ~ \"%s\"\n", $0 >>LOGFILE
                }
            }
            if (lines == "") {
                lines = "   " $0
            }
            else {
                lines = lines "\n   " $0
            }
        }
        END {
            finish_test()
            printf("\ntests:     %d\npassed:    %d\nfailed:    %d\n\n",
                   tests, passed, failed)
            printf("\ntests:     %d\npassed:    %d\nfailed:    %d\n\n",
                   tests, passed, failed) >>LOGFILE
            if (!complete) {
                print "NOT ALL TESTS COMPLETED!  (Check logfile for details)"
                print "NOT ALL TESTS COMPLETED!" >>LOGFILE
                exit(2)
            }
            else {
                if (tests != passed) {
                    exit(2)
                }
            }
        }
    ' LOGFILE=$1
}

# psql_clean
# Filter output from psql to clean it.  Send raw output to logfile $1
#
psql_clean()
{
    tee -a $1 | awk '
        function do_print(str) {
	    if (continuing) {
                printf("\n")
            }
            printf("%s", str)
            continuing = 1
        }
        /^-/ {
            sub(/^- ?/, "")
            do_print($0)
        }
	/^INSERT/ {printf(".")}
	/^CREATE/ {printf(".")}
	/^DROP/   {printf(".")}
	/^DELETE/ {printf(".")}
	/^COMMIT/ {printf(".")}
	/[:	]*ERROR[:	]/ {do_print($0); errors=1}
	/[:	]*HINT[:	]/ {do_print($0)}
	/[:	]*WARN[:	]/ {do_print($0)}
     	END {
	    do_print("")
            if (errors) exit(2)
	}   
    '
}
