#!/usr/bin/env python3
#仅参考测试，编译examples使用，请根据实际情况修改路径和命令
import sys
sys.dont_write_bytecode = True
import os
import subprocess
import click

from tools.scripts.build_mdk_project import build_mdk_project
from tools.scripts.after_build import _run_after_operations
from tools.scripts.before_build import _run_before_operations


# 你的 Keil 安装路径 (注意: 路径里如果反斜杠，建议前面加 r 或者用双反斜杠 \\)
repo_root = os.path.dirname(os.path.abspath(__file__))
demo_root = os.path.join(repo_root, "examples", "stm32g0_single_demo")
my_keil = r"C:\Users\w1545\AppData\Local\Keil_v5\UV4\UV4.exe"
boot_project = os.path.join(demo_root, "boot", "MDK-ARM", "stm32g0_single_slot_boot.uvprojx")
app_project = os.path.join(demo_root, "app", "MDK-ARM", "stm32g0_single_slot_app.uvprojx")
log_path = os.path.join(repo_root, "build")
firmware_elf = os.path.join(demo_root, "app", "MDK-ARM", "stm32g0_single_slot_app", "stm32g0_single_slot_app.axf")
firmware_hex = os.path.join(demo_root, "app", "MDK-ARM", "stm32g0_single_slot_app", "stm32g0_single_slot_app.hex")
ota_output = os.path.join(repo_root, "build", "stm32g0_single_slot_app.ota")
pack_script = os.path.join(repo_root, "scripts", "pack_ota.py")

def run_before_operations():
    return _run_before_operations(my_keil, app_project, log_path)

def run_after_operations():
    return _run_after_operations(my_keil, app_project, log_path)

def run_pack_ota():
    """执行 OTA 打包"""
    print("执行 OTA 打包...")
    cmd = [
        sys.executable,
        pack_script,
        "--elf",
        firmware_elf,
        "--firmware",
        firmware_hex,
        "--output",
        ota_output,
    ]
    subprocess.run(cmd, check=True)
    print(f"[OK] OTA 打包完成: {ota_output}")
    return 0

@click.group()
def cli():
    """这是主入口"""
    pass

@cli.command()
def build():
    """这是编译命令"""
    run_before_operations()
    build_mdk_project(my_keil, boot_project, log_path, "build")
    build_mdk_project(my_keil, app_project, log_path, "build")
    run_after_operations()
    run_pack_ota()
    

@cli.command()
def rebuild():
    """这是重编译命令"""
    
    build_mdk_project(my_keil, boot_project, log_path, "rebuild")
    build_mdk_project(my_keil, app_project, log_path, "rebuild")
    run_after_operations()
    run_pack_ota()


@cli.command()
def before():
    """这是构建前执行的命令，在Keil MDK构建前调用"""
    return run_before_operations()

@cli.command()
def after():
    """这是构建后执行的命令，在Keil MDK构建后调用"""
    return run_after_operations()

@cli.command()
def pack():
    """这是 OTA 打包命令"""
    return run_pack_ota()

if __name__ == '__main__':
    cli()
