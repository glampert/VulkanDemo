
### Basic commands to manage Git submodules

<pre>
Adding a new submodule - in the Git shell:
    cd Source/External
    git submodule add git@github.com:repo/foo.git
    git commit -am 'added foo module'
    git push

Updating existing submodules to latest:
    git submodule update --recursive --remote
</pre>

