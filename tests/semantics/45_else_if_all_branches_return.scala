import ifj26.*
@main def start(): Unit =
    print(sign(0))

def sign(x: Int): Int =
    if x < 0 then
        return -1
    else if x == 0 then
        return 0
    else
        return 1
