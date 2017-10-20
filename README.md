dospad
======

dosbox port for iOS, aka iDOS
# MSDOS for iOS 10 & Later

Updated app build settings to support iOS 10 or later. Initially this source code was built to run on older iOS 5 architectures, so hopefully this might run a whole lot faster on the latest hardware. 


## Getting Started
This process is ugly, because this app was designed for jailbroken iOS devices.
You will need to use this temporary hack to establish the right MSDOS C: drive directory.
The steps are summarized below:
* run the app first, copy the console output of the directory
* paste the output of the directory into main.m
* launch the app again, enter `dir` command to verify that it worked.

#### run the app first, copy the console output of the directory
copy the path displayed in between the purple hearts:
```
2017-10-20 02:47:07.902315-0400 dospad[3087:1415052] 💜
2017-10-20 02:47:07.902545-0400 dospad[3087:1415052] /var/mobile/Containers/Data/Application/BDB5BC8B-72EF-4EED-8265-C69FE7E21FC6/Documents
2017-10-20 02:47:07.902585-0400 dospad[3087:1415052] 💜
```
#### paste the output of the directory into main.m
Open `/Sources/dospad/Shared/main.m`, and scroll down to line 280 and replace the codeblock
with the following:
```
// Auto mount
#ifndef IDOS // DOSPAD for CYDIA
//        strcpy(diskc, "/var/mobile/Documents");
strcpy(diskc, "/var/containers/Bundle/Application/3FD50F1A-0E40-4F0E-A07A-8DBB3C91BBBB/");

strcpy(diskd, [[fso documentsDirectory] UTF8String]);
#else
strcpy(diskc, [[fso documentsDirectory] UTF8String]);
strcpy(diskd, "/var/containers/Bundle/Application/3FD50F1A-0E40-4F0E-A07A-8DBB3C91BBBB/");
#endif
```

#### launch the app again, enter `dir` command to verify that it worked.
Finally, run the app again and run the `dir` command. You should now be able
to view the contents of your root virtual C: drive.

## Supports Windows 95!
I have not tried this yet, but DOSBox might be the best emulator for running Windows 95.
Follow the getting started guide above, it should guide you on how to access your virtual
C: drive. You'll need to programatically copy a Windows95.img into that directory.
