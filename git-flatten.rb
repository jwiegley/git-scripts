#!/usr/bin/env ruby

require 'fileutils'
require 'open3'
require 'ftools'
require 'set'

$name = "git flatten"

def usage retcode=0
  puts <<USAGE
usage: #{$name} [options] <ref>
USAGE
  exit retcode
end

def die msg, usage=false
  $stderr.puts msg
  usage ? usage(-1) : exit(-1)
end

def execute cmd, opts={}
  verbose = opts[:verbose]
  debug   = opts[:debug]
  result  = opts[:return] || :to_s
  select  = opts[:select]
  filter  = opts[:filter]
  res = []
  puts cmd if debug
  IO.popen cmd do |f|
    f.each_line do |line|
      l = line.sub(/\n$/,'')
      puts l if debug
      if !select || select[l]
        l = filter ? filter[l] : l
        puts l if verbose
        res << l
      end
    end
  end
  if result == :result
    $?.success?
  else
    if opts[:one]
      res.empty? ? nil : res.first
    else
      res
    end
  end
end

def git_dir
  $git_dir ||= execute("git rev-parse --git-dir", :one => true)
end

def git_editor
  $git_editor ||= execute("git config core.editor", :one => true) || 
                  ENV['VISUAL'] || ENV['EDITOR'] || 'vi'
end

def git_branch
  $git_branch ||= execute("git branch", 
                          :select => lambda{|l| l =~ /^\*/}, 
                          :filter => lambda{|l| l.sub(/\* /, '')},
                          :one => true)
end

def ref_to_hash ref
  execute("git rev-parse #{ref}", :one => true)
end

def hash_to_str hash
  token = '--token--'
  execute("git log -1 --pretty=format:'%h#{token}%s' #{hash}", 
          :one => true).split(/#{token}/)
end

def rev_list *ref
  execute("git rev-list --reverse #{ref.join(' ')}")
end

def patch_id hash
  execute("git show #{hash} | git patch-id", :one => true)
end

def merge? hash
  patch_id(hash).nil?
end

def merge_base c1, c2
  execute("git merge-base #{c1} #{c2}", :one => true)
end

def container_branches hash
  execute("git branch -a --contains #{hash}",
          :filter => lambda{|b| b.sub(/^\s+/,'')})
end

def parent_branches hash
  parents = execute("git log -1 --pretty=format:%P #{hash}",
                    :one => true).split(/\s+/)
  branches = Set.new
  parents.each do |parent|
    branches |= container_branches(parent)
  end
  branches
end

def parse_flatten target, opts={}
  name = "flattenorig"
  dirname  = "#{git_dir}/refs/#{name}"
  filename = "#{dirname}/#{git_branch}"
  if Dir[dirname].empty?
    Dir.mkdir dirname
  end
  if Dir[filename].empty? && !opts[:write]
    File.open(filename, "w+") do |file|
      file.puts merge_base('HEAD', target)
    end
  end
  if opts[:read]
    File.open(filename) do |file|
      file.readline.sub(/\n$/,'')
    end
  elsif opts[:write]
    File.open(filename,"w") do |file|
      file.puts opts[:write]
    end
  else
    filename
  end
end


def interactive_edit ref, revs, squash, opts={}
  name = "FLATTEN"
  filename = "#{git_dir}/#{name}"
  limit = opts[:limit] || 70
  if opts[:delete]
    File.safe_unlink filename
  else
    return [] if revs.empty?
    unless File.exists? filename
      File.open(filename, "w+") do |file|
        str_HEAD = hash_to_str(ref)[0]
        str_revs  = revs.map do |rev|
          hash, str = hash_to_str(rev)
          str = str[0..(limit-3)] + '...' if str.length > limit
          [hash, str]
        end
        str_revs.each do |hash, str|
          if merge? hash
            if (parent_branches(hash) & squash).empty?
              action = :squash
            else
              action = :pick
            end
          else
            action = :pick
          end
          res = "%-6s %s %s" % [action, hash, str]
          file.puts res
        end
        file.puts <<USAGE
# Flatten #{str_HEAD}..#{str_revs.last[0]} onto #{str_HEAD}
#
# Commands:
#  p, pick = use commit
#  e, edit = use commit, but stop for amending
#  s, squash = use commit, but meld into previous commit
#
# If you remove a line here THAT COMMIT WILL BE LOST.
# However, if you remove everything, the rebase will be aborted.
#
USAGE
      end
    end
    system git_editor, filename
    res = []
    File.open(filename) do |file|
      file.each_line do |line|
        res << line.sub(/\n$/,'').split(/\s+/) unless line =~ /^#/
      end
    end
    res
  end
end

def store_last last=nil, opts={}
  name = "FLATTEN_LAST"
  filename = "#{git_dir}/#{name}"
  if opts[:delete]
    File.safe_unlink filename
  elsif last
    File.open(filename, "w+") do |file|
      file.puts last
    end
  else
    if File.exists? filename
      File.open(filename) do |file|
        file.gets.sub(/\n$/,'')
      end
    else
      nil
    end
  end
end

def parse_action action
  case action
  when /^e(dit)?$/i
    :edit
  when /^p(ick)$/i
    :pick
  when /^s(quash)$/i
    :squash
  else
    die "unknown action '#{action}'"
  end
end

def flatten ref, refs, opts={}
  squash = opts[:squash] || []
  orig = parse_flatten ref, :read => true
  target = rev_list ref, 
                    "^#{git_branch}", 
                    "^#{orig}", 
                    *[refs, squash.map{|s| "^#{s}"}].flatten
  stored_last = store_last
  last = stored_last
  revs = interactive_edit orig, target, squash
  revs.each do |action, abbrev, str|
    hash = ref_to_hash abbrev
    if stored_last
      puts "#{stored_last} / #{hash}"
      stored_last = nil if hash == stored_last
      next
    end
    action = parse_action action
    if action == :squash
      parse_flatten ref, :write => hash
      next
    end
    result = execute( "git merge -q --squash #{hash}",
                      :verbose => true,
                      :return => :result )
    die "" unless result
    parse_flatten ref, :write => hash
    store_last hash
    if action == :edit
      die "ok, you can edit it"
    else
      result = execute( "git commit -C #{hash}",
                        :verbose => true,
                        :return => :result)
      die "" unless result
    end
  end
  if last
    parse_flatten ref, :write => last
  else
    puts "nothing to do"
  end
  store_last nil, :delete => true
  interactive_edit nil, nil, nil, :delete => true
end


ref  = nil
refs = []
opts = {}
args = ARGV.dup
while arg = args.shift
  case arg
  when /-h|--help/
    usage
  when /-s|--squash/
    opts[:squash] ||= []
    opts[:squash] << (arg =~ /=/ ? arg.sub(/.*=\s*/,'') : args.shift )
  when /^-/
    die "unknow option #{arg}", true
  else
    case
    when ref
      refs << arg
    else
      ref = arg
    end
  end
end
usage -1 unless ref
flatten ref, refs, opts
