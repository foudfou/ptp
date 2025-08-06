import re

def check_data(expect, data, ctx):
    failure = 0
    patt = re.compile(expect, re.DOTALL)
    if re.search(patt, data):
        result = "OK"
        err = None
    else:
        failure += 1
        result = "\033[31mFAIL\033[0m"
        err = data
    print(f"{ctx['line']}/{ctx['len']} {ctx['name']} .. {result}")
    if err:
        print(f"---\n   data {data} did not match expected {expect}")
    return failure
