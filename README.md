An attempt at reimplementing the Nim [miho-driver] in C.

[miho-driver]: https://github.com/NelsonCrosby/miho-driver/tree/1ff8eb097cffec4fb7dc5987b5e68b523bb7b30f

I ultimately found it really frustrating
how Nim's asyncio didn't work outside of the main thread,
which I needed to be able to do
at the very least for the .NET interface implementation.
While not meant to be strictly threadsafe (yet?),
this implementation should be fine running on any given thread.


## Project ##

There are two targets generated by this project:

* `miho-driver.dll` is the primary output.
  Interfaces will call its functions directly.
* `miho-cli.exe` is a sample interface
  with minimal acutal user functionality.
  Its primary purpose is as a development tool,
  but it could be useful in some situations.
