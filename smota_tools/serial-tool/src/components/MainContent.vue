<script setup lang="ts">
import { ref, computed, watch, nextTick, onMounted, onUnmounted } from 'vue'
import type { LogInst } from 'naive-ui'
import { invoke } from '@tauri-apps/api/core'
import { listen, type UnlistenFn } from '@tauri-apps/api/event'
import { save } from '@tauri-apps/api/dialog'
import { writeTextFile, BaseDirectory } from '@tauri-apps/api/fs'
import { useSerialStore } from '../stores/serialStore'

// 日志项接口
interface LogItem {
    content: string      // 显示用的字符串
    raw: string         // 原始数据（用于 Hex 格式）
    time: string         // 格式: HH:mm:ss.SSS
    timestamp: number
    type: 'rx' | 'tx'   // 区分接收和发送
}

const store = useSerialStore()

// 接收数据列表
const receiveDataList = ref<LogItem[]>([])
// 显示RX开关
const showRX = ref(true)
// 显示TX开关
const showTX = ref(true)
// 自动滚动开关
const autoScroll = ref(true)
// Hex/ASCII 显示切换
const showHex = ref(false)
// 暂停显示开关
const pauseDisplay = ref(false)
// 最大日志数量
const MAX_LOG_COUNT = 1000
// 发送数据
const sendData = ref('')
// 发送中
const sending = ref(false)

// 事件监听器
let unlistenData: UnlistenFn | null = null
// NLog 组件实例
const logInstRef = ref<LogInst | null>(null)

// 格式化时间（带毫秒，格式 HH:mm:ss.SSS）
const formatTime = () => {
    const now = new Date()
    return now.toLocaleTimeString('zh-CN', { hour12: false }) + '.' +
           String(now.getMilliseconds()).padStart(3, '0')
}

// 推送日志并自动滚动
const pushLogItem = (item: LogItem) => {
    receiveDataList.value.push(item)
    if (receiveDataList.value.length > MAX_LOG_COUNT) {
        receiveDataList.value.shift()
    }

    // 如果暂停显示，则不触发滚动
    if (!pauseDisplay.value && autoScroll.value && logInstRef.value) {
        nextTick(() => {
            logInstRef.value?.scrollTo({ position: 'bottom', silent: true })
        })
    }
}

// 启动接收监听
const startReceiveListening = async () => {
    if (unlistenData) return
    unlistenData = await listen<string>('serial-data', (event) => {
        if (event.payload) {
            const newItem: LogItem = {
                content: event.payload,
                raw: event.payload,
                time: formatTime(),
                timestamp: Date.now(),
                type: 'rx'
            }
            pushLogItem(newItem)
        }
    })
}

// 停止接收监听
const stopReceiveListening = () => {
    if (unlistenData) {
        unlistenData()
        unlistenData = null
    }
}

// 发送数据
const sendDataToSerial = async () => {
    if (!sendData.value) return
    if (!store.isConnected) {
        console.error('串口未连接')
        return
    }

    sending.value = true
    try {
        await invoke('send_data', { data: sendData.value })
        // 存入发送日志
        const sendItem: LogItem = {
            content: sendData.value,
            raw: sendData.value,
            time: formatTime(),
            timestamp: Date.now(),
            type: 'tx'
        }
        pushLogItem(sendItem)
        sendData.value = ''
    } catch (error) {
        console.error('发送失败:', error)
    } finally {
        sending.value = false
    }
}

// 键盘事件 - 只拦截普通 Enter，让 Ctrl+Enter 走默认换行
const onKeydown = (e: KeyboardEvent) => {
    if (e.key === 'Enter' && !e.ctrlKey && !e.shiftKey) {
        e.preventDefault()
        sendDataToSerial()
    }
}

// 清空接收数据
const clearReceiveData = () => {
    receiveDataList.value = []
}

// 格式化日志供 NLog 使用
const formattedLogs = computed(() => {
    if (receiveDataList.value.length === 0) return ''

    const logs = receiveDataList.value
        .filter(item => (item.type === 'rx' && showRX.value) || (item.type === 'tx' && showTX.value))
        .map(item => {
            const data = showHex.value
                ? item.raw.split('').map(c => c.charCodeAt(0).toString(16).padStart(2, '0').toUpperCase()).join(' ')
                : item.content
            return `[${item.time}] [${item.type.toUpperCase()}] ${data}`
        })
        .join('\n')
    return logs + '\n'
})

// 导出日志
const exportLogs = async () => {
    try {
        // 调用后端生成日志内容
        const content = await invoke('export_logs', {
            logs: receiveDataList.value,
            format: showHex.value ? 'hex' : 'ascii'
        }) as string

        // 弹出保存对话框
        const fileName = `serial_log_${new Date().toISOString().slice(0, 19).replace(/:/g, '').replace('T', '_')}.log`
        const path = await save({
            defaultPath: fileName,
            filters: [{ name: 'Log', extensions: ['log', 'txt'] }]
        })

        if (path) {
            await writeTextFile(path, content, { dir: BaseDirectory.Document })
            console.log('日志已保存到:', path)
        }
    } catch (error) {
        console.error('导出失败:', error)
    }
}

onMounted(async () => {
    // 检查连接状态
    await store.checkConnection()
})

// 监听连接状态变化，自动启动/停止监听
watch(() => store.isConnected, (connected) => {
    if (connected) {
        startReceiveListening()
    } else {
        stopReceiveListening()
    }
}, { immediate: true })

onUnmounted(() => {
    stopReceiveListening()
})

// 暴露给外部使用
defineExpose({
    startReceiveListening,
    stopReceiveListening
})
</script>

<template>
    <div class="main-content flex flex-col h-full">
        <!-- 接收区 -->
        <div class="flex-1 flex flex-col min-h-0">
            <!-- 设置栏 -->
            <div class="flex items-center gap-4 mb-2">
                <div class="flex items-center gap-2">
                    <span class="text-xs text-gray-500">RX</span>
                    <NSwitch v-model:value="showRX" size="small" />
                </div>
                <div class="flex items-center gap-2">
                    <span class="text-xs text-gray-500">TX</span>
                    <NSwitch v-model:value="showTX" size="small" />
                </div>
                <div class="flex items-center gap-2">
                    <span class="text-xs text-gray-500">HEX</span>
                    <NSwitch v-model:value="showHex" size="small" />
                </div>
                <div class="flex items-center gap-2">
                    <span class="text-xs text-gray-500">暂停</span>
                    <NSwitch v-model:value="pauseDisplay" size="small" />
                </div>
                <div class="flex items-center gap-2">
                    <span class="text-xs text-gray-500">自动滚动</span>
                    <NSwitch v-model:value="autoScroll" size="small" />
                </div>
                <NButton size="tiny" @click="exportLogs">导出</NButton>
            </div>
            <!-- 接收区标题和清空按钮 -->
            <div class="flex justify-between items-center mb-2">
                <div class="flex items-center gap-2">
                    <div class="text-xs text-gray-500">接收区</div>
                    <NTag :bordered="false" type="info" size="small">
                        {{ receiveDataList.length }} 条
                    </NTag>
                </div>
                <NButton size="tiny" quaternary @click="clearReceiveData">清空</NButton>
            </div>
            <!-- 实际接收数据区（可滚动） -->
            <div class="flex-1 min-h-0 relative border border-gray-200 rounded bg-gray-50 overflow-hidden">
                <NLog
                    ref="logInstRef"
                    :log="formattedLogs"
                    :trim="false"
                    class="absolute inset-0 p-2 font-mono text-sm"
                />
            </div>
        </div>

        <!-- 发送区 -->
        <div class="flex-none flex items-center gap-2 h-24 mt-2">
            <NInput
                v-model:value="sendData"
                type="textarea"
                :autosize="{ minRows: 2, maxRows: 4 }"
                placeholder="输入数据 (Enter 发送)..."
                :disabled="!store.isConnected"
                @keydown="onKeydown"
            />
            <NButton
                type="primary"
                :loading="sending"
                :disabled="!store.isConnected || !sendData"
                @click="sendDataToSerial"
            >
                发送
            </NButton>
        </div>
    </div>
</template>

<style scoped>
.main-content {
    height: 100%;
}
</style>
