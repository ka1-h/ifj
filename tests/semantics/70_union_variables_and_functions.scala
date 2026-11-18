import ifj26.*

type Number = Int | Double
type Scalar = Number | String

def identity(value: Scalar): Scalar = value

@main def start(): Unit =
    var value: Scalar = 1
    value = "text"
    value = 2.5
    val copy: Scalar = identity(value)
    ()
