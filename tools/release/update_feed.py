#!/usr/bin/env python3
"""Generate the update feed automatically in GitHub Actions.

Production versions come from published release tags and matching assets; no
version is configured here. --self-test runs isolated synthetic release fixtures
without contacting GitHub or publishing anything. No local invocation is needed
for the normal release workflow.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import time
import urllib.error
import urllib.request

VERSION = re.compile(r'^v?(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$')
ASSET = re.compile(r'^KGroundControl_v(\d+\.\d+\.\d+)_(Windows|Ubuntu_\d+\.\d+|Debian_\d+(?:\.\d+)?)_(x64|x86|arm64)(\.exe)?$')


def request(url, token='', api=True):
    headers = {'User-Agent': 'KGroundControl-release-feed'}
    if api:
        headers['Accept'] = 'application/vnd.github+json'
        headers['X-GitHub-Api-Version'] = '2022-11-28'
        if token:
            headers['Authorization'] = f'Bearer {token}'
    for attempt in range(3):
        try:
            return urllib.request.urlopen(urllib.request.Request(url, headers=headers), timeout=60)
        except urllib.error.HTTPError as error:
            if error.code < 500 or attempt == 2:
                raise
        except urllib.error.URLError:
            if attempt == 2:
                raise
        time.sleep(2 ** attempt)


def pages(url, token):
    page = 1
    while True:
        with request(f'{url}?per_page=100&page={page}', token) as response:
            items = json.load(response)
        if not isinstance(items, list):
            raise ValueError(f'Expected an API list: {url}')
        yield from items
        if len(items) < 100:
            break
        page += 1


def platform_key(os_name, architecture):
    if os_name == 'Windows':
        return f'windows-{architecture}'
    return f'linux-{os_name.lower().replace("_", "-")}-{architecture}'


def generate(releases, repository, digest_resolver=None):
    """Order by semantic version per platform, never publication date or event tag."""
    selected = {}
    for release in releases:
        version = VERSION.fullmatch(release.get('tag_name', ''))
        if release.get('draft') or release.get('prerelease') or not version:
            continue
        numeric = tuple(map(int, version.groups()))
        normalized = '.'.join(map(str, numeric))
        for asset in release.get('assets', []):
            match = ASSET.fullmatch(asset.get('name', ''))
            if not match or match[1] != normalized or asset.get('state') != 'uploaded' or asset.get('size', 0) <= 0:
                continue
            _, os_name, arch, extension = match.groups()
            if (os_name == 'Windows') != (extension == '.exe'):
                continue
            expected = f'https://github.com/{repository}/releases/download/{release["tag_name"]}/{asset["name"]}'
            if asset.get('browser_download_url') != expected:
                continue
            key = platform_key(os_name, arch)
            rank = (numeric, int(asset.get('id', 0)))
            if key in selected and rank <= selected[key][0]:
                continue
            digest = asset.get('digest') or ''
            sha = digest.removeprefix('sha256:') if digest.startswith('sha256:') else ''
            if not re.fullmatch('[a-fA-F0-9]{64}', sha):
                sha = digest_resolver(asset) if digest_resolver else ''
            selected[key] = (rank, {
                'latest-version': normalized, 'download-url': expected,
                'changelog': release.get('body') or '', 'mandatory-update': False,
                'platform': key, 'architecture': arch,
                'size': asset['size'], 'sha256': sha.lower(),
            })
    updates = {key: value for key, (_, value) in sorted(selected.items())}
    # Compatibility for previously shipped clients. New clients use architecture keys.
    for key, entry in list(updates.items()):
        if key.endswith('-x64'):
            updates[key[:-4]] = entry
    if 'linux-ubuntu-22.04-x64' in updates:
        updates['linux'] = updates['linux-ubuntu-22.04-x64']
    return {'schema-version': 2, 'updates': updates}


# Synthetic release versions below are test fixtures, not production defaults.
# This function is used only by --self-test; generate() always reads API tags.
def self_test():
    repo = 'owner/repo'
    def release(version, platforms, **options):
        tag = 'v' + version
        assets = []
        for i, (os_name, arch) in enumerate(platforms):
            name = f'KGroundControl_v{version}_{os_name}_{arch}' + ('.exe' if os_name == 'Windows' else '')
            assets.append(dict(id=i, name=name, state='uploaded', size=100,
                digest='sha256:' + 'a' * 64,
                browser_download_url=f'https://github.com/{repo}/releases/download/{tag}/{name}'))
        return dict(tag_name=tag, assets=assets, **options)
    windows = [('Windows', 'x64')]
    linux = [('Ubuntu_24.04', 'x64')]
    releases = [release('1.4.6', linux), release('1.3.5', windows),
                release('1.4.0', linux), release('1.3.10', linux),
                release('1.3.2', windows), release('2.0.0', windows, prerelease=True),
                release('3.0.0', linux, draft=True), release('1.4.6', [('Ubuntu_24.04', 'arm64')])]
    feed = generate(releases, repo)['updates']
    assert feed['windows-x64']['latest-version'] == '1.3.5'
    assert feed['linux-ubuntu-24.04-x64']['latest-version'] == '1.4.6'
    assert feed['linux-ubuntu-24.04-arm64']['architecture'] == 'arm64'
    assert 'linux' not in feed
    assert generate(reversed(releases), repo) == generate(releases, repo)
    releases.append(release('1.4.6', windows))  # delayed Windows asset on an existing tag
    assert generate(releases, repo)['updates']['windows-x64']['latest-version'] == '1.4.6'
    older = generate([release('1.3.9', linux), release('1.3.10', linux)], repo)
    assert older['updates']['linux-ubuntu-24.04-x64']['latest-version'] == '1.3.10'
    print('PASS: staggered platforms, delayed assets, numeric ordering, major/minor jumps, architecture isolation, drafts/prereleases, order independence')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repository', default=os.environ.get('GITHUB_REPOSITORY', 'YevheniiKovryzhenko/KGroundControl'))
    parser.add_argument('--output', type=Path, default=Path('public/updates.json'))
    parser.add_argument('--digest-cache', type=Path, default=Path('.cache/update-feed/digests.json'))
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    if not re.fullmatch(r'[\w.-]+/[\w.-]+', args.repository):
        parser.error('Invalid repository')
    token = os.environ.get('GITHUB_TOKEN', '')
    base = f'https://api.github.com/repos/{args.repository}'
    releases = list(pages(base + '/releases', token))
    for release in releases:
        if not release.get('draft') and not release.get('prerelease') and VERSION.fullmatch(release.get('tag_name', '')):
            release['assets'] = list(pages(f'{base}/releases/{release["id"]}/assets', token))
    cache = json.loads(args.digest_cache.read_text()) if args.digest_cache.exists() else {}
    def digest(asset):
        key = f'{asset["id"]}:{asset.get("updated_at", "")}:{asset["size"]}'
        if key not in cache:
            h = hashlib.sha256()
            count = 0
            with request(asset['browser_download_url'], api=False) as response:
                while chunk := response.read(1024 * 1024):
                    count += len(chunk)
                    h.update(chunk)
            if count != asset['size']:
                raise ValueError('Incomplete asset download while hashing')
            cache[key] = h.hexdigest()
        return cache[key]
    feed = generate(releases, args.repository, digest)
    if not feed['updates']:
        raise ValueError('No valid release executables; refusing to replace the update feed')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(feed, indent=2) + '\n')
    args.digest_cache.parent.mkdir(parents=True, exist_ok=True)
    args.digest_cache.write_text(json.dumps(cache, sort_keys=True) + '\n')
    print(f'Generated {args.output}: {len(feed["updates"])} platform entries')


if __name__ == '__main__':
    main()
