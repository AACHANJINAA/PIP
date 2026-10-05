"""배포 폴더를 만든다.

저장소 루트의 PathManifest.json(별칭, 앱별 배포 범위, 실행 파일 목록)을 읽어 복사하고 exe 옆에 Deploy.json을 쓴다.
PathManager는 Deploy.json이 있을 때만 배포 모드로 동작한다. 설명: 기획 & 계획/PathAlias_Migration_KR.md

사용:
    python Tools/deploy.py                              # 클라·서버 Release를 Deploy/ 에
    python Tools/deploy.py --app server --config Debug
    python Tools/deploy.py --build                      # 빌드 후 배포
    python Tools/deploy.py --check                      # 사용 파일이 배포 범위에 있는지 검사

복사 규칙
    - 별칭마다 include.<앱>의 항목만 복사한다. "*"는 별칭 폴더 전체. 항목은 하위 폴더·파일 이름(와일드카드 가능)
    - 별칭 exclude와 전체 exclude에 맞는 파일·폴더는 뺀다
    - App 폴더(appRoots) 안의 별칭은 같은 상대 위치로, 밖의 별칭은 External/<별칭>/ 으로 복사한다
    - 폴더 복사는 robocopy /MIR (바뀐 파일만 복사, 원본에 없는 파일은 삭제)

--check: 개발 모드로 실행할 때 쌓이는 <App>/Saved/used_files.txt를 읽어,
         그 앱의 배포 범위 밖에서 읽은 파일을 출력한다(있으면 종료 코드 1)
"""
import argparse
import fnmatch
import json
import shutil
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SOLUTIONS = {"client": "Client/Client.sln", "server": "Server/Server.sln"}


def load_manifest():
    with open(REPO / "PathManifest.json", encoding="utf-8") as f:
        return json.load(f)


def includes(alias_def, app):
    return list(alias_def.get("include", {}).get(app, []))


def excludes(manifest, alias_def):
    return list(alias_def.get("exclude", [])) + list(manifest.get("exclude", []))


def is_excluded(rel_path, patterns):
    """경로의 구성 요소 중 하나라도 exclude 패턴에 맞으면 제외"""
    parts = [p for p in rel_path.replace("\\", "/").split("/") if p]
    return any(fnmatch.fnmatch(part.lower(), pat.lower()) for part in parts for pat in patterns)


def alias_dest_rel(alias_name, dev, app_root):
    """App 폴더 안이면 App 기준 상대 경로, 밖이면 External/<별칭>"""
    full = (REPO / dev).resolve()
    try:
        return full.relative_to(app_root.resolve()).as_posix()
    except ValueError:
        return f"External/{alias_name}"


def robocopy(src, dst, exclude):
    args = ["robocopy", str(src), str(dst), "/MIR", "/NFL", "/NDL", "/NJH", "/NJS", "/NP", "/R:1", "/W:1", "/MT:16"]
    if exclude:
        args += ["/XD", *exclude, "/XF", *exclude]
    code = subprocess.run(args, stdout=subprocess.DEVNULL).returncode
    if code >= 8:
        raise RuntimeError(f"robocopy 실패({code}): {src} -> {dst}")


def folder_size(path):
    return sum(f.stat().st_size for f in path.rglob("*") if f.is_file()) if path.exists() else 0


def format_size(n):
    return f"{n / 2**30:.2f} GB" if n >= 2**30 else f"{n / 2**20:.1f} MB"


def check(manifest, apps):
    aliases = sorted(manifest["aliases"].items(), key=lambda kv: len(kv[1]["dev"]), reverse=True)
    bad = 0
    for app in apps:
        used_path = REPO / manifest["appRoots"][app] / "Saved" / "used_files.txt"
        if not used_path.exists():
            print(f"[{app}] used_files.txt 없음 (개발 모드로 실행한 적 없음): {used_path}")
            continue

        lines = [l.strip() for l in used_path.read_text(encoding="utf-8").splitlines() if l.strip()]
        problems = []
        for line in lines:
            match = None
            for name, d in aliases:
                dev = d["dev"].rstrip("/")
                if line.lower().rstrip("/") == dev.lower() or line.lower().startswith(dev.lower() + "/"):
                    match = (name, d, dev)
                    break
            if not match:
                problems.append(f"별칭 밖       {line}")
                continue

            name, d, dev = match
            rel = line[len(dev):].strip("/")
            if not rel:
                continue  # 별칭 폴더 자체(디렉터리 순회)

            inc = includes(d, app)
            if not inc:
                problems.append(f"앱 범위 밖    {line}  (별칭 {name}에 include.{app} 없음)")
                continue
            first = rel.split("/")[0]
            if "*" not in inc and not any(fnmatch.fnmatch(first, i) or fnmatch.fnmatch(rel, i) for i in inc):
                problems.append(f"include 밖    {line}  (별칭 {name})")
            elif is_excluded(rel, excludes(manifest, d)):
                problems.append(f"exclude됨     {line}  (별칭 {name})")

        print(f"[{app}] 사용 파일 {len(lines)}개 검사: 문제 {len(problems)}개")
        for p in problems:
            print(f"  {p}")
        bad += len(problems)
    return 1 if bad else 0


def find_msbuild():
    vswhere = Path(r"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe")
    pattern = r"MSBuild\**\Bin\amd64\MSBuild.exe"
    # 프로젝트가 v143(VS 2022) 툴셋이라 VS 2022를 우선
    for extra in (["-version", "[17.0,18.0)"], ["-latest"]):
        out = subprocess.run([str(vswhere), *extra, "-requires", "Microsoft.Component.MSBuild", "-find", pattern],
                             capture_output=True, text=True).stdout.strip().splitlines()
        if out:
            return out[0]
    raise RuntimeError("MSBuild를 찾을 수 없음")


def build(apps, config):
    msbuild = find_msbuild()
    for app in apps:
        print(f"[{app}] 빌드 ({config})")
        code = subprocess.run([msbuild, str(REPO / SOLUTIONS[app]), f"/p:Configuration={config}",
                               "/p:Platform=x64", "/m", "/v:m", "/nologo"]).returncode
        if code != 0:
            raise RuntimeError(f"[{app}] 빌드 실패")


def deploy(manifest, apps, config, out_root):
    for app in apps:
        app_root = REPO / manifest["appRoots"][app]
        dest = out_root / app
        dest.mkdir(parents=True, exist_ok=True)
        print(f"[{app}] -> {dest}")

        # 실행 파일·DLL
        for bin_rel in manifest["binaries"][app][config]:
            src = REPO / bin_rel
            if not src.exists():
                raise RuntimeError(f"[{app}] 실행 파일 없음 (빌드 필요, --build): {src}")
            shutil.copy2(src, dest)

        # 별칭 폴더
        deploy_aliases = {}
        total = 0
        for name, d in manifest["aliases"].items():
            inc = includes(d, app)
            if not inc:
                continue
            src = REPO / d["dev"]
            if not src.exists():
                print(f"  경고: 별칭 {name} 폴더 없음: {src}")
                continue

            dest_rel = alias_dest_rel(name, d["dev"], app_root)
            alias_dest = dest / dest_rel
            exclude = excludes(manifest, d)

            for item in inc:
                if item == "*":
                    robocopy(src, alias_dest, exclude)
                    continue
                found = sorted(src.glob(item))
                if not found:
                    print(f"  경고: 별칭 {name}의 include 항목 없음: {item}")
                    continue
                for f in found:
                    if is_excluded(f.name, exclude):
                        continue
                    if f.is_dir():
                        robocopy(f, alias_dest / f.name, exclude)
                    else:
                        alias_dest.mkdir(parents=True, exist_ok=True)
                        shutil.copy2(f, alias_dest / f.name)

            deploy_aliases[name] = dest_rel
            size = folder_size(alias_dest)
            total += size
            print(f"  {name:<18} {dest_rel:<45} {format_size(size):>10}")

        # 배포 모드 표시 + 별칭 위치 (PathManager가 읽음)
        deploy_json = {"app": app, "config": config, "aliases": deploy_aliases}
        (dest / "Deploy.json").write_text(json.dumps(deploy_json, ensure_ascii=False, indent=2), encoding="utf-8")
        print(f"  합계 {format_size(total)}")


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    sys.stderr.reconfigure(encoding="utf-8")
    parser = argparse.ArgumentParser(description="PathManifest.json 기준으로 배포 폴더를 만든다")
    parser.add_argument("--app", choices=["client", "server", "all"], default="all")
    parser.add_argument("--config", choices=["Debug", "Release"], default="Release")
    parser.add_argument("--out", default="Deploy", help="출력 폴더 (상대 경로면 저장소 기준)")
    parser.add_argument("--build", action="store_true", help="배포 전에 MSBuild로 빌드")
    parser.add_argument("--check", action="store_true", help="used_files.txt가 배포 범위 안인지 검사만")
    args = parser.parse_args()

    manifest = load_manifest()
    apps = ["client", "server"] if args.app == "all" else [args.app]

    if args.check:
        return check(manifest, apps)
    if args.build:
        build(apps, args.config)
    out_root = Path(args.out)
    if not out_root.is_absolute():
        out_root = REPO / out_root
    deploy(manifest, apps, args.config, out_root)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except RuntimeError as e:
        print(f"오류: {e}", file=sys.stderr)
        sys.exit(1)
