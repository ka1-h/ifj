import ifj26.*
@main def start(): Unit =
    print(false && probe())
    print(true || probe())

def probe(): Boolean =
    print("X")
    true
