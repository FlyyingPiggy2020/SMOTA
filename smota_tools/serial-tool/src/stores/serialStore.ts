import { defineStore } from 'pinia'
import { ref } from 'vue'
import { invoke } from '@tauri-apps/api/core'

export const useSerialStore = defineStore('serial', () => {
  // 串口参数
  const baudRate = ref<number>(115200)
  const dataBits = ref<number>(8)
  const stopBits = ref<number>(1)
  const parity = ref<'none' | 'odd' | 'even'>('none')
  const port = ref<string>('')

  // 连接状态
  const isConnected = ref(false)

  // UI 状态
  const expandedNames = ref<string[]>(['serial-config'])
  const flowExpandedNames = ref<string[]>(['flow-control'])

  // 检查连接状态
  const checkConnection = async () => {
    try {
      const result = await invoke<{ status: string }>('check_connection_status')
      isConnected.value = result.status === 'Connected'
    } catch (error) {
      isConnected.value = false
    }
  }

  // 设置连接状态
  const setConnected = (connected: boolean) => {
    isConnected.value = connected
  }

  return {
    baudRate,
    dataBits,
    stopBits,
    parity,
    port,
    isConnected,
    expandedNames,
    flowExpandedNames,
    checkConnection,
    setConnected
  }
}, {
  persist: true
})
