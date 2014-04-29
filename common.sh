# default options
PREFIX= # empty means install into root
ARCH=k1om
CONFIG=1

# parse some extra arguments
while [ $# -gt 0 ]; do
	case "$1" in
	--no-config )
		CONFIG=0
		shift ;;
	-p  | --prefix )
		[ $# -ge 2 ] || { echo "error: '$1' requires parameter" &1>2; exit 1; }
		PREFIX="$2"
		shift 2 ;;
	-a  | --arch )
		[ $# -ge 2 ] || { echo "error: '$1' requires parameter" &1>2; exit 1; }
		ARCH="$2"
		shift 2 ;;
	-\? | --help )
		cat <<-END
		$0 [options]

		flags:
		  --no-config  do not install config files into /etc

		options:
		  -p, --prefix PREFIX   prefix for configuration files installation
		  -k, --kernel VERSION  kernel version [manual install] (auto-detected)
		  -a, --arch   ARCH     driver architecture (default: $ARCH)
		END
		exit 0 ;;
	* ) echo "error: unknown option '$1'" &1>2; exit 1 ;;
	esac
done

# require root
if [ $EUID -ne 0 ]; then
	echo "$0 must be run under as root. Try: 'sudo $0'" 1>&2
	exit 1
fi

# check if DKMS is present, otherwise show some instructions
if [ -z "$(which dkms 2>/dev/null)" ]; then
	echo -n "$0 expects DKMS to be installed." 1>&2
	if [ -n "$(which apt-get 2>/dev/null)" ]; then
		echo " Try: 'sudo apt-get install dkms'"
	elif [ -n "$(which yum 2>/dev/null)" ]; then
		echo " Try: 'sudo yum install dkms'"
	else
		echo
		echo "Install without DKMS using: 'make && sudo make install'"
	fi
	exit 1
fi

# get version information from Git tag
if [ -n "$(which git 2>/dev/null)" ]; then
	TAG=$(git describe)
	TAG=${TAG%-g*} # strip Git version if any
	VERSION=${TAG#*-}
	NAME=${TAG%%-*}
else # fallback to .mpss-metadata if Git is not present
	read VERSION < .mpss-metadata
	NAME=mpss
	TAG=$NAME-$VERSION
fi

# make sure VERSION and NAME are set
if [ -z "$VERSION" ] || [ -z "$NAME" ]; then
	echo "Cannot find module version. Aborting." 1>&2
	exit 1
fi

SRC="/usr/src/$TAG"
