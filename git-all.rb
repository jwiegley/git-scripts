#!/usr/bin/env ruby

# Report the status of all Git repositories within a directory tree.

require 'main'
require 'git'

$params = []

trap('INT') { exit 1 }

module GitUpdate
  @sep_printed = false
  @last_empty = false

  def GitUpdate.separator
    if @sep_printed
      if not @last_empty
        print "\n"
      end
      @last_empty = false
    else
      @sep_printed = true
    end
  end

  def GitUpdate.top_ten category, message, lines, second_flag=false, marker='='
    return if $params[:fetch].value and marker == '='

    if not lines.empty? or ($params[:verbose].value and not second_flag)
      separator
      puts marker * 2 + " #{category} " + marker * 2 + "  #{message}"
      if lines.empty?
        @last_empty = true
      else
        puts lines.first(10).map { |line| line[0...80] }
        if lines.size > 10
          puts "... (and #{lines.size - 10} more)"
        end
      end
    end
  end

  def GitUpdate.git_svn_repo g
    g.config['svn-remote.svn.url'] rescue false
  end

  def GitUpdate.fetch g, working_dir
    case
    when git_svn_repo(g)
      lines = `git --git-dir='#{@git_dir}' svn fetch 2>&1`.split("\n")
      lines.reject! do |line|
        line =~ /^(W: |This may take a while|Checked through|$)/
      end
      lines.compact!

      top_ten 'SVN FETCH', working_dir, lines
    else
      begin
        options = '-q --progress --all --prune --tags'
        out = `git --git-dir='#{@git_dir}' fetch #{options} 2>&1`
        top_ten 'FETCH', working_dir, out.split("\n")

      rescue Git::GitExecuteError => msg
        puts "Error: #{msg}"
      end
    end
  end

  def GitUpdate.status working_dir
    options = '--porcelain'
    if $params[:untracked].value
      options << ' --untracked-files=normal'
    else
      options << ' --untracked-files=no'
    end

    changes = `git --git-dir='#{@git_dir}' status #{options}`

    top_ten 'STATUS', working_dir, changes.split("\n"), $params[:fetch].value
  end

  def GitUpdate.push_or_pull g, working_dir, branch
    remote = g.config["branch.#{branch.name}.remote"] || git_svn_repo(g)
    if remote
      remote_sha =
        if git_svn_repo(g)
          g.revparse 'trunk' rescue g.revparse 'git-svn'
        else
          g.revparse File.join(remote, branch.name)
        end

      if branch.gcommit.sha != remote_sha
        git_log = "git --git-dir='#{@git_dir}' log --no-merges --oneline"

        out = `#{git_log} #{remote_sha}..#{branch.gcommit.sha}`
        top_ten('NEED PUSH', "#{working_dir}\##{branch.name}",
            out.split("\n"), true)

        if $params[:pulls].value
          out = `#{git_log} #{branch.gcommit.sha}..#{remote_sha}`
          top_ten('NEED PULL', "#{working_dir}\##{branch.name}",
              out.split("\n"), true)
        end
      end
    end
  end

  def GitUpdate.report working_dir
    logger = Logger.new(STDOUT)
    logger.level = Logger::WARN

    g = Git.open(working_dir, :log => logger)
    @git_dir = File.join working_dir, '.git'

    if $params[:fetch].value or $params[:fetchonly].value
      fetch g, working_dir
    end

    if not $params[:fetchonly].value
      g.branches.local.each { |b| push_or_pull g, working_dir, b }
      status working_dir
    end
  end
end

Main {
  argument(:dir) {
    arity -1
    default '.'
    description 'Directories to scan'
  }

  option(:fetch) {
    description 'Fetch from all remotes'
  }
  option(:errors) {
    description 'Only show Git failures'
  }
  option(:fetchonly) {
    description 'Only fetch, nothing else'
  }
  option(:pulls) {
    description 'Include NEED PULL sections'
  }
  option(:untracked) {
    description 'Display untracked files as possible changes'
  }
  option(:verbose)

  def run
    $params  = params
    seen     = Set.new
    dirs     = $params[:dir].values * ' '
    # Relies on GNU find's -printf directive
    find_cmd = "find -H #{dirs} -name .git -printf '%p\\n\\c'"

    # This asynchronous usage allows the "find" command to continue producing
    # pathnames while we work on them.  Find.find() would block until all the
    # pathnames had been found, which can be very slow (especially on laptop
    # hard drives with large filesets).

    IO.popen(find_cmd).each do |path|
      path = File.absolute_path path

      next if seen.include? path # never process the same path twice
      seen << path

      begin
        GitUpdate.report File.dirname(path)
      rescue => ex
        GitUpdate.top_ten('FAILED', path, [ex.message] + ex.backtrace,
                      false, '#')
      end
      STDOUT.flush
    end
  end
}

### git-all ends here
