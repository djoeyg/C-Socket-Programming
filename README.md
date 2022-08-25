# C-Socket-Programming
One Time Pads: Client &amp; Server Socket connections that encrypt &amp; decrypt plain text messages

The one-time pad (OTP) is an encryption technique that cannot be cracked, but requires the use of a single-use pre-shared key that is not smaller than the message being sent. In this technique, a plaintext is paired with a random secret key (also referred to as a one-time pad). Then, each bit or character of the plaintext is encrypted by combining it with the corresponding bit or character from the pad using modular addition.<br />
(Source: https://en.wikipedia.org/wiki/One-time_pad).

This set of programs encrypts and decrypts plaintext to and from ciphertext, using a key that generates strings of characters consisting of the 26 capital letters, and the space character. All 27 characters will be encrypted and decrypted using the technique of modular addition.

## Files &amp; Usage:

### Encryption Server `enc_server`
Performs encoding of the message received by the `enc_client` program. The server listens, accepts &amp; validates connections over the network on a given port. It then receives plaintext and a key from the client, encodes and sends the cipher message back over the network.

Command line args:  (0) ./enc_server  (1) arbitrary port number

### Encryption Client `enc_client`
Connects to `enc_server`, and asks it to perform a one-time pad style encryption. Receives ciphertext back from `enc_server` and outputs to stdout. If `enc_client` receives key or plaintext files with any bad characters, or the key file is shorter than the plaintext, the program terminates and sends appropriate error text to stderr. `enc_client` will not be able to connect to the decryption server, even if it tries to connect on the correct port.

Must start `enc_server` before running this program.<br />
Command line args:  (0) ./enc_client  (1) plaintext file name  (2) key file name  (3) port number from the server

### Decryption Server `dec_server`
Same functionality as `enc_server` except performs decoding of the cipher text message received by the `dec_client` program.

Command line args:  (0) ./dec_server  (1) arbitrary port number

### Decryption Client `dec_client`
Same functionality as `enc_client` except connects to `dec_server`, and asks it to perform a one-time pad style decryption. `dec_client` will not be able to connect to the encryption server, even if it tries to connect on the correct port.

Must start `dec_server` before running this program.<br />
Command line args:  (0) ./dec_client  (1) ciphertext file name  (2) key file name  (3) port number from the server

### `keygen`
A stand-alone utility that creates a key file of specified length. The characters in the file generated will be any of the 27 allowed characters, generated using the standard Unix randomization methods.

Command line args: (0) ./keygen  (1) length of key (positive integer value)