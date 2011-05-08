# Android Receiver

### Description

Listen for broadcast messages from [android-notifer][] coming over a 
specific port. Parse the message, format it, and hand it off for 
display.

This was done mostly as a learning exercise in C.

### Usage

    android-receiver [ --port <port> ] --handler <handler>

Port is optional and defaults to 10600. The handler you defined will be 
called asynchronously with the formatted message as the first and only 
argument.

You can find an example handler using dzen2 in my scripts repo.

### Note

My machine is setup to block TCP broadcast packets as per Arch's "Simple 
stately firewall". Rather than change this setting, I've elected to 
setup the notifier app on my phone to send a UDP packet to a specific IP 
address instead.

That is what this program expects.

[android-notifer]: http://code.google.com/p/android-notifier/ "android notifier on google code"
