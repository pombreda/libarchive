from _extension import archive_stream, archive_entry
from pyarchive import errors

class Archive(object):

    @classmethod
    def from_filepath(kls, filepath):
        obj = kls(archive_stream(filepath))
        obj.filepath = filepath
        return obj

    @classmethod
    def from_stream(kls, stream):
        return kls(stream)

    def __init__(self, obj):
        self.filepath = None
        if not isinstance(obj, archive_stream):
            if isinstance(obj, basestring):
                self.filepath = obj
            obj = archive_stream(obj)

        self._items = []
        self._stream = obj

    def __iter__(self):
        for entry in self._items:
            yield entry
        for entry in self._stream:
            self._items.append(entry)
            yield entry

    def _regenerate_stream(self):
        if not isinstance(self.filepath, basestring):
            raise errors.UsageError("can't reopen stream for %r" % (self,))
        self._stream = archive_stream(self.filepath)

    def extractfile(self, instance):
        if instance.header_position > len(self._items):
            raise errors.UsageError("instance %s isn't from this archive" % (instance,))
        if instance.header_position < self._stream.header_position:
            self._regenerate_stream()
        if instance.header_position != self._stream.header_position:
            siter = iter(self._stream)
            try:
                while instance.header_position > self._stream.header_position:
                    val = siter.next()
            except StopIteration:
                raise errors.PyArchiveError("can't find %s in %r" % (instance, self))
        assert self._stream.header_position == instance.header_position
        print instance.header_position, self._stream.header_position
        return self._stream.extractfile(instance)


def open(filepath):
    return Archive.from_filepath(filepath)
