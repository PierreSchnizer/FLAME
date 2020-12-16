#!../build/vaccel/flamedemo

dbLoadDatabase("../build/vaccel/flamedemo.dbd")
flamedemo_registerRecordDeviceDriver(pdbbase)

flamePrepare("SIM", "machine.lat")

dbLoadRecords("demo.db", "SIM=SIM, P=TST:")

iocInit()
