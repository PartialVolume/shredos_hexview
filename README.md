# shredos_hexviewer
Hex/ascii viewer for ShredOS

## How To Compile

#### Debian/Ubuntu
```
sudo apt install libncurses-dev
```
#### Fedora / RHEL / CentOS
```
sudo dnf install ncurses-devel
```
#### then
```
./autogen.sh
./configure
make
```
## Main Screen
<img width="1203" height="451" alt="Screenshot_20261016_232446" src="https://github.com/user-attachments/assets/6a0fddf3-9760-4605-9cd5-7403b9a94209" />

## Multiple Search Methods - full sequential or random chunk with selectable scope, percentage, MB, GB
<img width="1205" height="452" alt="Screenshot_20261016_232826" src="https://github.com/user-attachments/assets/2b181cf6-18be-4f64-a5ff-cb771c142a0e" />

## Goto - percentage or offset
<img width="1203" height="453" alt="Screenshot_20261016_233127" src="https://github.com/user-attachments/assets/d4c07b7f-6abc-49c8-af45-2bfcdf813ea4" />

## Shannon entropy Scan 0 to 8 (8 is best for random data)
What the Shannon score means:

- 0.0000: Plain zeros (0x00) or unformatted empty space.
- 1.0 to 6.9: Structured files, file systems, operating system code, or compressed data (which matches your 6.2788 score—typical of an active partition containing standard files, filesystem tables, or application binaries).
- 7.0 to 7.8: High randomness or encrypted partitions.
- 7.9 to 8.0: Maximum random noise (what you'd see after a cryptographically secure erase pass or NIST SP 800-88 compliant random overwrite).

<img width="1203" height="453" alt="Screenshot_20261016_233342" src="https://github.com/user-attachments/assets/b9572fa4-9a1a-460b-9a70-70a1f8db3f21" />

## Detection of Partition Type
<img width="1203" height="453" alt="Screenshot_20261016_233439" src="https://github.com/user-attachments/assets/fd6a4a41-2aba-409c-9481-aad45c59ee55" />




