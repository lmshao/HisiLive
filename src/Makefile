# Hisilicon Hi3516A+OV4689 Makefile

# Define cross compiler
CC = arm-hisiv300-linux-gcc

INC_DIR = ../include
LIB_DIR = ../lib
COMMON_DIR = ./common

# target source
SRC := $(wildcard *.c) 
OBJ := $(SRC:%.c=%.o)

# Hisilicon SDK sample_common source
COMM_SRC := $(wildcard $(COMMON_DIR)/*.c)
COMM_OBJ := $(COMM_SRC:%.c=%.o)

TARGET := HisiLive
.PHONY : clean all

all: $(TARGET)

INC_FLAGS = -I$(INC_DIR) -I$(COMMON_DIR)

CFLAGS = -Wall -g \
         -lpthread -lm -ldl \
         -mcpu=cortex-a7 \
         -mfloat-abi=softfp \
         -mfpu=neon-vfpv4 \
         -ffunction-sections \
         -mno-unaligned-access \
         -fno-aggressive-loop-optimizations \
         -Dhi3516a \
         -DHICHIP=0x3516A100 \
         -DHI_RELEASE \
         -DHI_XXXX \
         -DISP_V2 \
         -DHI_ACODEC_TYPE_INNER \
         -DSENSOR_TYPE=OMNIVISION_OV4689_MIPI_1080P_30FPS   # SENSOR_TYPE

CFLAGS += $(INC_FLAGS)

MPI_LIBS = $(LIB_DIR)/libmpi.a

AUDIO_LIBA =  $(LIB_DIR)/libVoiceEngine.a \
              $(LIB_DIR)/libupvqe.a \
              $(LIB_DIR)/libdnvqe.a

SENSOR_LIBS = $(LIB_DIR)/libisp.a \
              $(LIB_DIR)/lib_cmoscfg.a \
              $(LIB_DIR)/lib_iniparser.a \
              $(LIB_DIR)/lib_hiae.a \
              $(LIB_DIR)/lib_hiawb.a \
              $(LIB_DIR)/lib_hiaf.a \
              $(LIB_DIR)/lib_hidefog.a \
              $(LIB_DIR)/libsns_ov4689.a


$(TARGET): $(OBJ) $(COMM_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(MPI_LIBS) $(AUDIO_LIBA) $(SENSOR_LIBS)

clean:
	@rm -f $(TARGET)
	@rm -f $(OBJ)
	@rm -f $(COMM_OBJ)

cleanstream:
	@rm -f *.h264
	@rm -f *.jpg
	@rm -f *.mjp
	@rm -f *.mp4
