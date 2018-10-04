# Miho PC Remote - Driver #

_Miho_ is an open source PC remote control. It implements input controls.
The goal is to make it easier to use certain PC functions when keyboard
and mouse is too inconvenient (such as when connected to a TV).

The driver implements the protocol and executes the commands (e.g. actually
moving and clicking the mouse). There is also a command-line program to run
the driver without requiring any kind of interface.

The actual "remote" part is implemented by a client app, located in a separate
project:

* Android: [`miho-client-android`](https://github.com/NelsonCrosby/miho-client-android)

The name is a reference to Miho Noyama, a character from _A Channel_. I won't
explain the joke, if you wanna understand, go watch it. Or not, I'm not your
mother.


> ## WARNING ##
>
> Miho is currently not only prototype-quality, but also very much insecure.
> Specifically, the protocol is raw and unencrypted, which carries risks.
> I do plan to encrypt it soon, but right now that has not yet happened.


## TODO ##

- [x] Simple mouse move and button control
- [x] Extensible protocol and architecture
- [x] Stubs for testing without really affecting the host
  or requiring platform support
- [x] Client library (again for testing)
- [ ] More stable error handling
  - ~~Right now there's a lot of assertions for protocol-related things that the
    client is expected to get right. These need to be translated to proper
    error handling (probably by just terminating the connection, maybe with a
    diagnostic for client debugging).~~
  - Most of the assertions have been removed, and there's a much better error
    handling system now. There's still a couple of assertions for parts of
    CBOR that I haven't yet implemented, but they are still handled.
- [ ] More options for running from command-line (e.g. port)
- [ ] SSH transport layer
- [ ] Begin more user-friendly interface (will be separate project)
- [ ] Keyboard support

### Future directions ###

- App-specific drivers (e.g. browser plugin for Soundcloud)
- Linux support
