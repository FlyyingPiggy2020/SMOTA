<script setup lang="ts">
import { ref, onMounted, onUnmounted } from "vue";
import { invoke } from "@tauri-apps/api/core";
import {
    NButton,
    NCard,
    NSelect,
    NInput,
    NSwitch,
    NGrid,
    NGridItem,
} from "naive-ui";
import type { SerialPortInfo, SerialConfig } from "./types/serialTypes";
import { serialService } from "./services/serialService";

// 串口列表
const portList = ref<SerialPortInfo[]>([]);
const selectedPort = ref<string | null>(null);
const isConnected = ref(false);
const isConnecting = ref(false);

// 串口配置
const baudRateOptions = [
    { label: "9600", value: 9600 },
    { label: "19200", value: 19200 },
    { label: "38400", value: 38400 },
    { label: "57600", value: 57600 },
    { label: "115200", value: 115200 },
    { label: "230400", value: 230400 },
    { label: "460800", value: 460800 },
];
const baudRate = ref(115200);

const dataBitsOptions = [
    { label: "5", value: 5 },
    { label: "6", value: 6 },
    { label: "7", value: 7 },
    { label: "8", value: 8 },
];
const dataBits = ref(8);

const stopBitsOptions = [
    { label: "1", value: 1 },
    { label: "2", value: 2 },
];
const stopBits = ref(1);

const parityOptions = [
    { label: "None", value: "none" },
    { label: "Odd", value: "odd" },
    { label: "Even", value: "even" },
];
const parity = ref("none");

// 数据收发
const receiveData = ref("");
const sendDataText = ref("");
const isHexMode = ref(false);
const autoScroll = ref(true);
const receiveCount = ref(0);
const sendCount = ref(0);

// 接收定时器
let receiveTimer: number | null = null;

// 加载串口列表
const loadPorts = async () => {
    try {
        portList.value = await serialService.getPortList();
    } catch (error) {
        console.error("加载串口列表失败:", error);
    }
};

// 连接/断开串口
const toggleConnection = async () => {
    if (isConnected.value) {
        // 断开连接
        if (receiveTimer) {
            clearInterval(receiveTimer);
            receiveTimer = null;
        }
        await serialService.close();
        isConnected.value = false;
    } else {
        // 连接
        if (!selectedPort.value) {
            alert("请选择串口");
            return;
        }

        isConnecting.value = true;
        try {
            const config: Partial<SerialConfig> = {
                baud_rate: baudRate.value,
                data_bits: dataBits.value,
                stop_bits: stopBits.value,
                parity: parity.value,
            };

            await serialService.open(selectedPort.value, config);
            isConnected.value = true;

            // 开始轮询接收数据
            receiveTimer = setInterval(pollReceive, 100);
        } catch (error) {
            alert(`连接失败: ${error}`);
        } finally {
            isConnecting.value = false;
        }
    }
};

// 轮询接收数据
const pollReceive = async () => {
    if (!isConnected.value) return;

    try {
        const data = await serialService.receive(1024);
        if (data) {
            receiveData.value += data;
            receiveCount.value += data.length;
        }
    } catch (error) {
        console.error("接收数据失败:", error);
    }
};

// 发送数据
const sendData = async () => {
    if (!isConnected.value) {
        alert("请先连接串口");
        return;
    }

    if (!sendDataText.value) {
        return;
    }

    try {
        const bytes = await serialService.send(sendDataText.value);
        sendCount.value += bytes;
        sendDataText.value = ""; // 发送后清空
    } catch (error) {
        alert(`发送失败: ${error}`);
    }
};

// 清除接收区
const clearReceive = () => {
    receiveData.value = "";
    receiveCount.value = 0;
};

// 刷新串口列表
const refreshPorts = () => {
    loadPorts();
};

onMounted(() => {
    loadPorts();
});

onUnmounted(() => {
    if (receiveTimer) {
        clearInterval(receiveTimer);
    }
});
</script>

<template>
    <div class="container">
        <h1 class="title">串口调试工具</h1>

        <!-- 串口连接区 -->
        <n-card title="串口连接" class="card">
            <n-grid :cols="4" :x-gap="12">
                <n-grid-item :span="2">
                    <n-select
                        v-model:value="selectedPort"
                        placeholder="选择串口"
                        :options="portList.map(p => ({
                            label: `${p.port_name} (${p.port_type})`,
                            value: p.port_name
                        }))"
                        :disabled="isConnected"
                        @update:value="refreshPorts"
                    />
                </n-grid-item>
                <n-grid-item>
                    <n-button @click="refreshPorts" :disabled="isConnected">
                        刷新
                    </n-button>
                </n-grid-item>
                <n-grid-item>
                    <n-button
                        type="primary"
                        :loading="isConnecting"
                        @click="toggleConnection"
                    >
                        {{ isConnected ? "断开" : "连接" }}
                    </n-button>
                </n-grid-item>
            </n-grid>

            <!-- 串口参数 -->
            <n-grid :cols="4" :x-gap="12" style="margin-top: 12px">
                <n-grid-item>
                    <n-select
                        v-model:value="baudRate"
                        placeholder="波特率"
                        :options="baudRateOptions"
                        :disabled="isConnected"
                    />
                </n-grid-item>
                <n-grid-item>
                    <n-select
                        v-model:value="dataBits"
                        placeholder="数据位"
                        :options="dataBitsOptions"
                        :disabled="isConnected"
                    />
                </n-grid-item>
                <n-grid-item>
                    <n-select
                        v-model:value="stopBits"
                        placeholder="停止位"
                        :options="stopBitsOptions"
                        :disabled="isConnected"
                    />
                </n-grid-item>
                <n-grid-item>
                    <n-select
                        v-model:value="parity"
                        placeholder="校验位"
                        :options="parityOptions"
                        :disabled="isConnected"
                    />
                </n-grid-item>
            </n-grid>
        </n-card>

        <!-- 数据收发区 -->
        <n-card title="数据收发" class="card">
            <!-- 接收区 -->
            <div class="receive-section">
                <div class="section-header">
                    <span>接收区</span>
                    <div class="header-controls">
                        <label>
                            <n-switch v-model:checked="isHexMode" size="small" />
                            Hex 模式
                        </label>
                        <label>
                            <n-switch v-model:checked="autoScroll" size="small" />
                            自动滚动
                        </label>
                        <span class="count">接收: {{ receiveCount }} 字节</span>
                        <n-button size="small" @click="clearReceive">清空</n-button>
                    </div>
                </div>
                <n-input
                    v-model:value="receiveData"
                    type="textarea"
                    placeholder="接收的数据将显示在这里..."
                    :rows="10"
                    readonly
                    class="receive-area"
                />
            </div>

            <!-- 发送区 -->
            <div class="send-section">
                <div class="section-header">
                    <span>发送区</span>
                    <span class="count">发送: {{ sendCount }} 字节</span>
                </div>
                <n-input-group>
                    <n-input
                        v-model:value="sendDataText"
                        type="textarea"
                        placeholder="输入要发送的数据..."
                        :rows="3"
                        @keydown.enter.ctrl="sendData"
                    />
                    <n-button type="primary" @click="sendData" style="height: auto">
                        发送
                    </n-button>
                </n-input-group>
                <div class="hint">提示: Ctrl+Enter 发送</div>
            </div>
        </n-card>
    </div>
</template>

<style>
.container {
    padding: 16px;
    max-width: 1200px;
    margin: 0 auto;
}

.title {
    text-align: center;
    margin-bottom: 16px;
    font-size: 24px;
    font-weight: bold;
}

.card {
    margin-bottom: 16px;
}

.section-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 8px;
}

.header-controls {
    display: flex;
    align-items: center;
    gap: 16px;
}

.count {
    color: #666;
    font-size: 12px;
}

.receive-section,
.send-section {
    margin-bottom: 16px;
}

.receive-area {
    font-family: monospace;
}

.hint {
    font-size: 12px;
    color: #999;
    margin-top: 4px;
}
</style>
