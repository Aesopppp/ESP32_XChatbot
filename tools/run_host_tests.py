"""Run upstream host tests on Windows without changing their assertions."""
import os
from pathlib import Path
import shlex
import shutil
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent
os.chdir(ROOT)

# 上游若干测试切换到临时目录后直接退出上下文。Windows 不允许删除进程
# 当前目录，因此在临时目录清理前返回项目根目录；不修改测试中的断言。
original_cleanup = tempfile.TemporaryDirectory.cleanup


def cleanup_from_valid_directory(directory):
    current = Path.cwd().resolve()
    temporary = Path(directory.name).resolve()
    if current == temporary or temporary in current.parents:
        os.chdir(ROOT)
    original_cleanup(directory)


tempfile.TemporaryDirectory.cleanup = cleanup_from_valid_directory
suite = unittest.defaultTestLoader.discover(str(ROOT / 'scripts' / 'tests'))
cxx = shlex.split(os.environ.get('CXX', 'c++'))
if not cxx or not shutil.which(cxx[0]):
    import test_ogg_demuxer
    test_ogg_demuxer.OggDemuxerTest = unittest.skip(
        'No native host C++ compiler; ESP32 cross compiler cannot run this host executable'
    )(test_ogg_demuxer.OggDemuxerTest)

result = unittest.TextTestRunner(verbosity=2).run(suite)
sys.exit(0 if result.wasSuccessful() else 1)
