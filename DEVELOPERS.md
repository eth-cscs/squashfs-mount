## Workflow

The development workflow is simple: develop feature, enhancement and fix its own separate branch, then make a pull request.
* don't merge PRs without one aproving review
* don't merge PRs if all tests don't pass
* use squash merges so that there is a single commit to the `master` for each PR that represents a tested and working version of the software.

### Releases

Release frequently, following semantic versioning.

To release, update the `VERSION` file to create and create a tag.
```bash
# edit the VERSION file, e.g. from '0.4.0-dev' to '0.4.0'
vim VERSION
# make a PR that bumps the version
git ...

# then create a tag and push.
git tag -a "v$(echo VERSION)" -m "tag and release version $(echo version)"
git push origin "v$(echo VERSION)"

# Then update the VERSION file to dev status again e.g. to '0.5.0-dev'
# And make a PR
git ...
```

## CI/CD

There are two GitHub Actions workflows for testing that:
1. the makefile and meson build systems can build, install working versions of squashfs-mount.
2. the workflow for creating artifacts (currently a source RPM).

All workflows run on each push and PR. Furthermore, the artifacts workflow, [`.github/workflows/artifacts.yml`](https://github.com/eth-cscs/squashfs-mount/blob/master/.github/workflows/artifacts.yml), runs a `tag-release` job when a tag is created, that automatically generates a GitHub release and uploads the artifacts to the release.

