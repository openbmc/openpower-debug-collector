## Building the Code

```
To build this package, do the following steps:

    1. meson build
    2. ninja -C build

To clean the repository run `rm -rf build`.
```

## Host dump collection interface

The host dump collection interface facilitates the initiation of dump collections programmatically. To invoke this interface for various dump types, including hostboot dumps, use the following command structure:

```bash
busctl --verbose call org.open_power.Dump.Manager \
       /org/openpower/dump xyz.openbmc_project.Dump.Create \
       CreateDump a{sv} 2 \
       "com.ibm.Dump.Create.CreateParameters.DumpType" s \
       "com.ibm.Dump.Create.DumpType.<DUMPTYPE>" \
       "com.ibm.Dump.Create.CreateParameters.ErrorLogId" t  <ERROR_LOG_ID>
       "com.ibm.Dump.Create.CreateParameters.FailingUnitId" t <FAILING_UNIT_ID>
```
### Parameters:
<DUMPTYPE>: This is the type of dump you wish to collect. For hostboot dumps, replace <DUMPTYPE> with Hostboot. The complete list of valid dump types can be found [here](https://github.com/openbmc/phosphor-dbus-interfaces/blob/master/yaml/com/ibm/Dump/Create.interface.yaml).
<ERROR_LOG_ID>: This is a 32-bit number representing the error log ID associated with the dump.
<FAILING UNIT ID>: is an 8bit number indicating the id of the failing processor
  and the maximum allowed value is 32.
