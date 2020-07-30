# Mailpunk
A C++ email client.

## Getting started
Clone this repository and compile the code by running the following commands:
```
mkdir build && cd build/
cmake ..
cmake --build .
```

## Sending test emails

```
(echo From: $USER@`hostname`; echo "Subject: A test email"; echo; echo "Here is the first line of the body"; 􏰀→ echo "and the second") | curl smtp://mailpunk.lsds.uk --mail-rcpt ${USER}mail@localhost -T -
```
