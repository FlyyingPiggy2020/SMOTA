<script setup lang="ts">
import { ref, onMounted, computed } from 'vue'
import { invoke } from '@tauri-apps/api/core'
import { useSerialStore } from '../stores/serialStore'

const store = useSerialStore()

// 端口选项（动态加载）
const portOptions = ref<Array<{ label: string; value: string }>>([])
const loading = ref(false)

// 连接中状态（本地使用，不持久化）
const connecting = ref(false)

// 使用计算属性从 store 获取真实连接状态
const isConnected = computed(() => store.isConnected)

// 刷新端口列表
const refreshPorts = async () => {
    loading.value = true
    try {
        const ports = await invoke<Array<{ name: string; port_type: string }>>('list_ports')
        portOptions.value = ports.map(p => ({
            label: `${p.name} (${p.port_type})`,
            value: p.name
        }))
    } catch (error) {
        console.error('获取串口列表失败:', error)
    } finally {
        loading.value = false
    }
}

// 打开/关闭串口
const toggleConnection = async () => {
    if (store.isConnected) {
        // 关闭串口
        try {
            await invoke('close_serial')
            await store.checkConnection()
        } catch (error) {
            console.error('关闭串口失败:', error)
        }
    } else {
        // 打开串口
        if (!store.port) {
            console.error('请选择串口')
            return
        }
        connecting.value = true
        try {
            await invoke('open_serial', {
                port: store.port,
                baudRate: store.baudRate,
                dataBits: store.dataBits,
                stopBits: store.stopBits,
                parity: store.parity
            })
            await store.checkConnection()
        } catch (error) {
            console.error('打开串口失败:', error)
        } finally {
            connecting.value = false
        }
    }
}

onMounted(async () => {
    refreshPorts()
    await store.checkConnection()
})

// 波特率选项（常用值）
const baudRateOptions = [
    { label: '300', value: 300 },
    { label: '600', value: 600 },
    { label: '1200', value: 1200 },
    { label: '2400', value: 2400 },
    { label: '4800', value: 4800 },
    { label: '9600', value: 9600 },
    { label: '14400', value: 14400 },
    { label: '19200', value: 19200 },
    { label: '28800', value: 28800 },
    { label: '38400', value: 38400 },
    { label: '57600', value: 57600 },
    { label: '115200', value: 115200 },
    { label: '230400', value: 230400 },
    { label: '460800', value: 460800 },
    { label: '921600', value: 921600 },
];

// 自定义输入的波特率
const customBaudRate = ref<number>(0)
// 是否显示自定义输入框
const showCustomInput = ref(false)
// 输入框引用
const inputRef = ref<HTMLInputElement | null>(null)

// 双击切换自定义输入模式
const onBaudRateDoubleClick = () => {
    customBaudRate.value = store.baudRate
    showCustomInput.value = true
    // 下次渲染后聚焦输入框
    setTimeout(() => {
        inputRef.value?.focus()
    }, 0)
}

// 完成自定义输入
const onCustomBaudRateConfirm = () => {
    const num = customBaudRate.value
    if (!isNaN(num) && num > 0) {
        store.baudRate = num
    }
    showCustomInput.value = false
}

// 键盘事件处理
const onCustomBaudRateKeydown = (e: KeyboardEvent) => {
    if (e.key === 'Enter') {
        onCustomBaudRateConfirm()
    } else if (e.key === 'Escape') {
        showCustomInput.value = false
    }
}

const dataBitsOptions = [
    { label: '7', value: 7 },
    { label: '8', value: 8 },
];

const stopBitsOptions = [
    { label: '1', value: 1 },
    { label: '2', value: 2 },
];

const parityOptions = [
    { label: '无', value: 'none' },
    { label: '奇校验', value: 'odd' },
    { label: '偶校验', value: 'even' },
];

</script>

<template>
    <NScrollbar style="max-height: calc(100vh - 100px);">
        <div class="p-4">
            <div class="flex justify-between items-center mb-4">
                <!-- 状态标签 -->
                <div class="text-sm">
                    <span :class="isConnected ? 'text-green-500' : 'text-gray-500'">●</span>
                    {{ isConnected ? '已连接' : '未连接' }}
                </div>
                <!-- 刷新端口按钮 -->
                <NButton size="small" :loading="loading" @click="refreshPorts">刷新端口</NButton>
            </div>
            <!-- 打开/断开按钮 -->
            <NButton type="primary" block class="mb-4!" :loading="connecting" @click="toggleConnection">
                {{ isConnected ? '关闭串口' : '打开串口' }}
            </NButton>

            <!-- 卡片1: 串口参数配置 -->
            <NCard size="small" class="mb-4">
                <NCollapse v-model:expanded-names="store.expandedNames">
                    <NCollapseItem title="串口参数配置" name="serial-config">
                        <NGrid :cols="1" :x-gap="4" :y-gap="6">
                            <!-- 端口号 -->
                            <NGi>
                                <label class="block text-sm mb-1">端口号</label>
                                <NSelect v-model:value="store.port" placeholder="选择端口" :options="portOptions" />
                            </NGi>

                            <!-- 波特率 -->
                            <NGi>
                                <label class="block text-sm mb-1">波特率</label>
                                <NInputNumber
                                    v-if="showCustomInput"
                                    ref="inputRef"
                                    v-model:value="customBaudRate"
                                    class="w-full"
                                    :min="0"
                                    :max="4294967295"
                                    placeholder="输入波特率"
                                    @blur="onCustomBaudRateConfirm"
                                    @keydown="onCustomBaudRateKeydown"
                                />
                                <NSelect
                                    v-else
                                    :options="baudRateOptions"
                                    :value="store.baudRate"
                                    @update:value="(val: number) => store.baudRate = val"
                                    @dblclick="onBaudRateDoubleClick"
                                />
                            </NGi>

                            <!-- 数据位 -->
                            <NGi>
                                <label class="block text-sm mb-1">数据位</label>
                                <NSelect v-model:value="store.dataBits" :options="dataBitsOptions" />
                            </NGi>

                            <!-- 停止位 -->
                            <NGi>
                                <label class="block text-sm mb-1">停止位</label>
                                <NSelect v-model:value="store.stopBits" :options="stopBitsOptions" />
                            </NGi>

                            <!-- 校验位 -->
                            <NGi>
                                <label class="block text-sm mb-1">校验位</label>
                                <NSelect v-model:value="store.parity" :options="parityOptions" />
                            </NGi>
                        </NGrid>
                    </NCollapseItem>
                </NCollapse>
            </NCard>

            <!-- 卡片2: 流控信号控制 -->
            <NCard size="small">
                <NCollapse v-model:expanded-names="store.flowExpandedNames">
                    <NCollapseItem title="流控信号控制" name="flow-control">
                        <NGrid :cols="3" :x-gap="16">
                            <!-- DTR -->
                            <NGi>
                                <div class="text-center">
                                    <div class="font-bold">DTR</div>
                                    <div class="text-gray-500 text-xs">数据就绪</div>
                                    <NSwitch class="mt-2" />
                                </div>
                            </NGi>

                            <!-- RTS -->
                            <NGi>
                                <div class="text-center">
                                    <div class="font-bold">RTS</div>
                                    <div class="text-gray-500 text-xs">请求发送</div>
                                    <NSwitch class="mt-2" />
                                </div>
                            </NGi>

                            <!-- BREAK -->
                            <NGi>
                                <div class="text-center">
                                    <div class="font-bold">BREAK</div>
                                    <div class="text-gray-500 text-xs">中断信号</div>
                                    <NSwitch class="mt-2" />
                                </div>
                            </NGi>
                        </NGrid>
                    </NCollapseItem>
                </NCollapse>
            </NCard>

        </div>
    </NScrollbar>
</template>
