PROGRAM					= led_dimmer
EXTRA_COMPONENTS		= extras/httpd extras/mbedtls extras/dhcpserver extras/spiffs extras/mcpwm
PROGRAM_SRC_DIR			= . ./common ./main ./cmdsvr
EXTRA_CFLAGS			= -DLWIP_HTTPD_CGI=1 -DLWIP_HTTPD_SSI=1 -DLWIP_HTTPD_SSI_INCLUDE_TAG=0 -I./fsdata
EXTRA_CFLAGS			+=-DLWIP_IGMP -DLWIP_RAND=rand -DLWIP_DHCP=1
#EXTRA_CFLAGS			+=-DLWIP_DEBUG=1 -DHTTPD_DEBUG=1

FLASH_SIZE				= 32
SPIFFS_BASE_ADDR		= 0x200000
SPIFFS_SIZE				= 0x010000
SPIFFS_SINGLETON		= 1
	
ESPPORT					?= /dev/ttyUSB0
ESBAUD					?= 115200

include ../esp-open-rtos/common.mk

$(eval $(call make_spiffs_image,files))

html:
	@echo "Generating fsdata.."
	cd fsdata && ./makefsdata
