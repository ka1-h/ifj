import ifj26.*
@main def start(): Unit =
    print(combine(left(), right()))
    ()

def left(): Int =
    print("L")
    1

def right(): Int =
    print("R")
    2

def combine(a: Int, b: Int): Int = a * 10 + b
