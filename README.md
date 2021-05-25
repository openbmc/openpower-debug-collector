## To Build
```
To build this package, do the following steps:

    1. ./bootstrap.sh
    2. ./configure ${CONFIGURE_FLAGS} --prefix=/usr
    3. make

To clean the repository run `./bootstrap.sh clean`.
```

```
For local build, pass --prefix=/usr option to the configure script
to let the Makefile use /usr/share value over /usr/local/share for ${datadir}
variable. The error yaml files and elog parser are stored in /usr/share
location in the SDK.
```

```
For CI build, ${datadir} is expanded to /usr/local/share to which the
dependent files and parser are exported, so don't add the prefix option.
```

```
For recipe build, ${datadir} is expanded to /usr/share to which the
dependent files and parser are exported, so don't add the prefix option.
```

## Host dump collection interface

The host dump collection interface can be invoked in the following way

```
busctl --verbose call org.open_power.Dump.Manager
       /org/openpower/dump xyz.openbmc_project.Dump.Create
       CreateDump a{ss} 2
       "com.ibm.Dump.Create.CreateParameters.DumpType"
       "com.ibm.Dump.Create.DumpType.<DUMPTYPE>"
       "com.ibm.Dump.Create.CreateParameters.ErrorLogId" “<ERROR LOG ID>"
```

Where
- DUMPTYPE is a valid dump type listed [here](https://github.com/openbmc/phosphor-dbus-interfaces/blob/master/yaml/com/ibm/Dump/Create.interface.yaml)
- ERROR LOG ID is a 32bit decimal number as string.
  - An error will be logged and dump will be collected with error log id as 0
    if an invalid number is passed.
