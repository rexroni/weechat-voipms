# WeeChat-VoIPms

A [WeeChat](https://weechat.org) plugin to send and receive SMS messages from
your [VoIP.ms](https://voip.ms) account.

## Motivation for Project

There are a variety of reasons to use a VoIP provider.  VoIP.ms is neat because
they support sending and receiving text messages, but the web portal they
provide suffers from a variety of shortfalls: draconian logout timers, no
auto-refresh, no submitting with "enter" key, etc.

On the other hand, WeeChat is an open-source chat client with a great
interface, and an excellent plugin infrastructure.  This plugin uses the
fantastic [PJSIP](https://www.pjsip.org) library to allow you to use WeeChat to
send and recieve SMS messages through your VoIP.ms account.

## Project Features

### Implemented

- Send and recieve (plain) text messages through WeeChat
- Start new conversations with a WeeChat command: `/sms NUMBER message...`
- WeeChat alerts when you receive a message
- Builds and runs on Linux (MacOS possible but untested)

### Not yet implemented

- Phone calls
- Fetching "missed" messages of message history
- Integration with WeeChat config system
- Support for a contacts list
- Sending or receiving of multi-media messages
- Group messages

### Known Bugs

- WeeChat needs a re-draw (control-L) after plugin loads
- Plugin segfaults or hangs on during `/plugin unload voipms`
- Crappy handling of SIP errors

## Configuring your VoIP.ms account

1. Log into https://voip.ms
1. On the "Sub Accounts" -> "Create Sub Account" page, create a new sub account.
   - Take note of the username, which will be a combination of your main user
     account and a new tag.  That whole username will be used as the "USERNAME"
     in the plugin.
   - The password you enter here will be used as the "PASSWORD" in the plugin.
   - Set the "CallerID Number" to be the DID number you are going to use.
   - Technically, you can just use your main account with this plugin (and not
     create a sub account), but if you use any other SIP software, each
     software needs its own sub account, or exciting things happen.
1. On the "DID Numbers" -> "Manage DID(s)" page, "edit" the DID you wish to use
   - Take note of the DID point of presence server, which will be used as the
     "REALM" in the plugin
   - check the "Short Message Service(SMS): Enable SMS" box
   - check the "SMS SIP Account (beta)" box, and make sure that the subaccount
     selected is the subaccount you just created.

## Configuring and Building the plugin

1. Install prequisites: `weechat-dev` and `libpjproject-dev`
1. Copy `config.h.orig` to `config.h` and edit it:
   - USERNAME should be the full username for your sub account
   - PASSWORD should be the password for the sub account
   - REALM should be the server specified in the settings for this DID
1. run `make`
1. copy or link  `voipms.so` into `~/.weechat/plugins`
1. start WeeChat, the plugin should autoload

## Using the plugin

- Receiving a message from a new phone number will create a new WeeChat buffer
- Replying in that buffer will reply to that phone number
- New conversations are started with the command: `/sms NUMBER message...`

## License

This code is in the public domain, under the [Unlicense](https://unlicense.org).
