# JoltViewer 실행 도우미
# 더블클릭하면 콘솔에서 포커스 위치와 기록 파일 경로를 입력받아 JoltViewer를 실행한다.
# - 포커스: "x,y,z" 또는 "x y z". 클라이언트 로그 줄([PhysicsDebugCapture] 뷰어: ... -focus=...)을 통째로 붙여넣어도 된다.
# - 파일: 비우면 이 폴더 또는 Client/Client의 client_physics_dump.bin. 탐색기에서 끌어다 놓아도 된다(따옴표 자동 제거).

import os
import re
import shutil
import subprocess
import sys
import tempfile

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
VIEWER_EXE = os.path.join(SCRIPT_DIR, "JoltViewer.exe")
DUMP_NAME = "client_physics_dump.bin"
# 기본 기록 파일: 이 폴더에 옮겨 둔 파일이 있으면 그것, 없으면 클라이언트 작업 폴더의 파일
DEFAULT_DUMP = next(
    (path for path in (os.path.join(SCRIPT_DIR, DUMP_NAME),
                       os.path.normpath(os.path.join(SCRIPT_DIR, "..", "..", "Client", "Client", DUMP_NAME)))
     if os.path.isfile(path)),
    os.path.normpath(os.path.join(SCRIPT_DIR, "..", "..", "Client", "Client", DUMP_NAME)))

NUMBER = r"[-+]?\d+(?:\.\d+)?(?:[eE][-+]?\d+)?"


def parse_focus(text):
    """입력에서 x, y, z 세 숫자를 찾는다. 비어 있으면 None(원점)."""
    text = text.strip()
    if not text:
        return None

    # 로그 줄을 붙여넣은 경우 -focus= 뒤만 사용
    match = re.search(r"-focus=(\S+)", text)
    if match:
        text = match.group(1)

    numbers = re.findall(NUMBER, text)
    if len(numbers) != 3:
        raise ValueError("숫자 3개(x, y, z)가 필요합니다: " + text)
    return tuple(float(n) for n in numbers)


def parse_path(text):
    path = text.strip().strip('"').strip("'")
    return os.path.normpath(path) if path else DEFAULT_DUMP


def prepare_dump(path):
    """JoltViewer는 인자를 공백으로 나누므로, 경로에 공백이 있으면 임시 폴더로 복사해서 연다."""
    if " " not in path:
        return path
    copy_path = os.path.join(tempfile.gettempdir(), "JoltViewer_" + os.path.basename(path).replace(" ", "_"))
    shutil.copyfile(path, copy_path)
    print("경로에 공백이 있어 복사본으로 엽니다: " + copy_path)
    return copy_path


def main():
    if not os.path.isfile(VIEWER_EXE):
        print("JoltViewer.exe를 찾을 수 없습니다: " + VIEWER_EXE)
        return False

    print("=== JoltViewer 실행 ===")
    while True:
        try:
            focus = parse_focus(input("포커스 위치 (x,y,z / 로그 줄 붙여넣기 / 비우면 원점): "))
            break
        except ValueError as error:
            print(error)

    while True:
        path = parse_path(input("기록 파일 (비우면 " + DEFAULT_DUMP + "): "))
        if os.path.isfile(path):
            break
        print("파일이 없습니다: " + path)

    args = [VIEWER_EXE]
    if focus is not None:
        args.append("-focus={:g},{:g},{:g}".format(*focus))
    args.append(prepare_dump(path))

    print("실행: " + " ".join(args))
    subprocess.Popen(args, cwd=SCRIPT_DIR)
    return True


if __name__ == "__main__":
    ok = main()
    if not ok:
        input("엔터를 누르면 종료합니다...")
        sys.exit(1)
