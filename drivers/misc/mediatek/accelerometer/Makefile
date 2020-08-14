# In case the platform does NOT support this type of sensors
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hwmon/include

obj-y   +=  accel.o accel_factory.o

obj-$(CONFIG_MTK_BMA253)   +=  bma253/
obj-$(CONFIG_MTK_LSM6DS3G)   +=  lsm6ds3g/
obj-$(CONFIG_MTK_MC3416)   +=  mc3416/
