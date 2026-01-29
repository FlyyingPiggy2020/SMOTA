# smOTA Makefile
# Copyright (c) 2026 by Lu Xianfan.

# 项目名称
PROJECT = smota

# 目录定义
SRC_DIR = smota
CORE_DIR = $(SRC_DIR)/smota_core
HAL_DIR = $(SRC_DIR)/smota_hal
CRYPTO_DIR = $(SRC_DIR)/smota_crypto
BOOTLOADER_DIR = $(SRC_DIR)/smota_bootloader

# 源文件
CORE_SRCS = $(wildcard $(CORE_DIR)/src/*.c)
HAL_SRCS = $(wildcard $(HAL_DIR)/ports/*/*.c)
CRYPTO_SRCS = $(wildcard $(CRYPTO_DIR)/src/*.c)
BOOTLOADER_SRCS = $(wildcard $(BOOTLOADER_DIR)/src/*.c)

# 所有源文件
SRCS = $(CORE_SRCS) $(CRYPTO_SRCS)
OBJS = $(SRCS:.c=.o)

# 头文件路径
INCLUDES = \
	-I$(SRC_DIR) \
	-I$(CORE_DIR)/inc \
	-I$(HAL_DIR)/inc \
	-I$(CRYPTO_DIR)/inc \
	-I$(BOOTLOADER_DIR)/inc

# 编译器设置
CC = gcc
CFLAGS = -Wall -Wextra -O2 $(INCLUDES)
LDFLAGS =

# 目标
.PHONY: all clean

all: $(OBJS)
	@echo "smOTA 编译完成"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS)
	@echo "清理完成"
