import { test, expect, type Page } from '@playwright/test';

test.describe('串口工具下拉框测试', () => {
  test.beforeEach(async ({ page }) => {
    // 导航到应用
    await page.goto('http://localhost:1420');
    // 等待页面加载
    await page.waitForLoadState('networkidle');
  });

  test('波特率下拉框应该可点击并显示选项', async ({ page }) => {
    // 定位波特率下拉框
    const baudRateSelect = page.locator('.n-grid').first().locator('.n-select');

    // 点击下拉框
    await baudRateSelect.click();

    // 等待一下让菜单出现
    await page.waitForTimeout(500);

    // 验证波特率选项存在
    await expect(page.getByText('115200')).toBeVisible({ timeout: 5000 });
  });

  test('串口选择下拉框应该显示检测到的串口', async ({ page }) => {
    // 等待串口列表加载
    await page.waitForFunction(() => {
      const selects = document.querySelectorAll('.n-select');
      for (const select of selects) {
        if (select.textContent && select.textContent.includes('COM')) {
          return true;
        }
      }
      return false;
    }, { timeout: 10000 });

    // 点击串口选择下拉框
    const portSelect = page.locator('body').locator('.n-select').first();
    await portSelect.click();

    // 验证串口选项出现
    await expect(page.getByText(/COM\d/)).toBeVisible({ timeout: 5000 });
  });

  test('点击下拉框不应产生严重错误', async ({ page }) => {
    const consoleErrors: string[] = [];

    // 监听控制台错误
    page.on('console', (msg) => {
      if (msg.type() === 'error') {
        consoleErrors.push(msg.text());
      }
    });

    // 点击波特率下拉框
    const baudRateSelect = page.locator('.n-grid').first().locator('.n-select');
    await baudRateSelect.click();

    // 等待一下让 Vue 处理
    await page.waitForTimeout(500);

    // 过滤出严重的错误（排除已知的 Teleport 警告）
    const seriousErrors = consoleErrors.filter(e =>
      !e.includes('Slot') &&
      !e.includes('Teleport') &&
      !e.includes('vue') &&
      !e.includes('Vue')
    );

    console.log('Console errors found:', seriousErrors);

    // 不应该有任何严重错误
    expect(seriousErrors.length).toBe(0);
  });
});

test.describe('Tauri 窗口测试', () => {
  test('页面应该正确加载所有组件', async ({ page }) => {
    await page.goto('http://localhost:1420');
    await page.waitForLoadState('networkidle');

    // 检查标题
    await expect(page.getByText('串口调试工具')).toBeVisible();

    // 检查串口连接卡片存在
    await expect(page.getByText('串口连接')).toBeVisible();

    // 检查数据收发卡片存在
    await expect(page.getByText('数据收发')).toBeVisible();

    // 检查下拉框存在
    const selects = page.locator('.n-select');
    await expect(selects.first()).toBeVisible();
  });
});
