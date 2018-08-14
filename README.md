# DataProtector

A simple ransomware defender for windows.

## How does it work ?

It uses minifilter to filt "rewrite" and "delete" events with filename's suffix in kernel.And it handles events in user mode by counting a process's behavior in 30s.If a process tried to delete or rewrite more than 5 files in 30 seconds,it will display a notice messagebox.

## How much can it do now ?

Defend all ransomware so far,unless it bypasses us by following ways.

## How to bypass protector ?

* rewrite(or delete) less than 5 files in 30s.
* rename a file then rewrite(or delete) it.
* kill user mode handler.
* add itself to whitelist.
* repeatly rewrite(or delete) a file in new process.
* ransomware injects explorer.exe then delete or rewrite files.

## How to avoid bypass ?

1. detach file type in kernel instead of matching suffix.
2. protect user handler and user mode whitelist file.
3. verify certification of a execution program,if it is signed,pass it,if not,record process tree,check if tree is trusted.
4. protect injection by other defender.

## How to use it ?

From now on,I have no certification for driver.So it's a test demo.
Firstly,install the driver by loadMiniFilterDriver.exe 
`loadMiniFilterDriver.exe install dp \path\to\dataproctorDriver.sys`

Then run dataproctorUser.exe.

## TODO

avoid bypass method.
