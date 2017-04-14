/* map_handle.hpp
A handle to a source of mapped memory
(C) 2016 Niall Douglas http://www.nedprod.com/
File Created: August 2016


Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

#ifndef BOOST_AFIO_MAP_HANDLE_H
#define BOOST_AFIO_MAP_HANDLE_H

#include "file_handle.hpp"

//! \file map_handle.hpp Provides `map_handle`

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)  // dll interface
#endif

BOOST_AFIO_V2_NAMESPACE_EXPORT_BEGIN

/*! \class section_handle
\brief A handle to a source of mapped memory.

\note On Windows the native handle of this handle is that of the NT kernel section object. On POSIX it is
a cloned file descriptor of the backing storage.
*/
class BOOST_AFIO_DECL section_handle : public handle
{
public:
  using extent_type = handle::extent_type;
  using size_type = handle::size_type;

  //! The behaviour of the memory section
  BOOSTLITE_BITFIELD_BEGIN(flag){none = 0,          //!< No flags
                                 read = 1 << 0,     //!< Memory views can be read
                                 write = 1 << 1,    //!< Memory views can be written
                                 cow = 1 << 2,      //!< Memory views can be copy on written
                                 execute = 1 << 3,  //!< Memory views can execute code

                                 nocommit = 1 << 8,     //!< Don't allocate space for this memory in the system immediately
                                 prefault = 1 << 9,     //!< Prefault, as if by reading every page, any views of memory upon creation.
                                 executable = 1 << 10,  //!< The backing storage is in fact an executable program binary.

                                 // NOTE: IF UPDATING THIS UPDATE THE std::ostream PRINTER BELOW!!!

                                 readwrite = (read | write)};
  BOOSTLITE_BITFIELD_END(flag)

protected:
  io_handle *_backing;
  extent_type _length;
  flag _flag;

public:
  //! Default constructor
  section_handle()
      : _backing(nullptr)
      , _length(0)
      , _flag(flag::none)
  {
  }
  //! Construct a section handle using the given native handle type for the section and the given i/o handle for the backing storage
  explicit section_handle(native_handle_type sectionh, io_handle *backing, extent_type maximum_size, flag __flag)
      : handle(sectionh, handle::caching::all)
      , _backing(backing)
      , _length(maximum_size)
      , _flag(__flag)
  {
  }
  //! Implicit move construction of section_handle permitted
  section_handle(section_handle &&o) noexcept : handle(std::move(o)), _backing(o._backing), _length(o._length), _flag(o._flag)
  {
    o._backing = nullptr;
    o._length = 0;
    o._flag = flag::none;
  }
  //! Move assignment of section_handle permitted
  section_handle &operator=(section_handle &&o) noexcept
  {
    this->~section_handle();
    new(this) section_handle(std::move(o));
    return *this;
  }
  //! Swap with another instance
  void swap(section_handle &o) noexcept
  {
    section_handle temp(std::move(*this));
    *this = std::move(o);
    o = std::move(temp);
  }

  /*! \brief Create a memory section.
  \param backing The handle to use as backing storage. An invalid handle means to use the system page file as the backing storage.
  \param maximum_size The maximum size this section can ever be. Zero means to use backing.length().
  \param _flag How to create the section.

  \errors Any of the values POSIX dup() or NtCreateSection() can return.
  */
  //[[bindlib::make_free]]
  static BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC result<section_handle> section(file_handle &backing, extent_type maximum_size = 0, flag _flag = flag::read | flag::write) noexcept;
  //! \overload
  //[[bindlib::make_free]]
  static inline result<section_handle> section(extent_type maximum_size, file_handle &backing, flag _flag = flag::read | flag::write) noexcept { return section(backing, maximum_size, _flag); }

  //! Returns the memory section's flags
  flag section_flags() const noexcept { return _flag; }
  //! Returns the borrowed handle backing this section, if any
  io_handle *backing() const noexcept { return _backing; }
  //! Returns the borrowed native handle backing this section
  native_handle_type backing_native_handle() const noexcept { return _backing ? _backing->native_handle() : native_handle_type(); }
  //! Return the current maximum permitted extent of the memory section.
  extent_type length() const noexcept { return _length; }

  /*! Resize the current maximum permitted extent of the memory section to the given extent.

  \errors Any of the values NtExtendSection() can return. On POSIX this is a no op.
  */
  //[[bindlib::make_free]]
  result<extent_type> truncate(extent_type newsize) noexcept;
};
inline std::ostream &operator<<(std::ostream &s, const section_handle::flag &v)
{
  std::string temp;
  if(!!(v & section_handle::flag::read))
    temp.append("read|");
  if(!!(v & section_handle::flag::write))
    temp.append("write|");
  if(!!(v & section_handle::flag::cow))
    temp.append("cow|");
  if(!!(v & section_handle::flag::execute))
    temp.append("execute|");
  if(!!(v & section_handle::flag::nocommit))
    temp.append("nocommit|");
  if(!!(v & section_handle::flag::prefault))
    temp.append("prefault|");
  if(!!(v & section_handle::flag::executable))
    temp.append("executable|");
  if(!temp.empty())
  {
    temp.resize(temp.size() - 1);
    if(std::count(temp.cbegin(), temp.cend(), '|') > 0)
      temp = "(" + temp + ")";
  }
  else
    temp = "none";
  return s << "afio::section_handle::flag::" << temp;
}

/*! \class map_handle
\brief A handle to a memory mapped region of memory.

\note The native handle returned by this map handle is always that of the backing storage, but closing this handle
does not close that of the backing storage, nor does releasing this handle release that of the backing storage.
Locking byte ranges of this handle is therefore equal to locking byte ranges in the original backing storage.
*/
class BOOST_AFIO_DECL map_handle : public io_handle
{
public:
  using path_type = io_handle::path_type;
  using extent_type = io_handle::extent_type;
  using size_type = io_handle::size_type;
  using mode = io_handle::mode;
  using creation = io_handle::creation;
  using caching = io_handle::caching;
  using flag = io_handle::flag;
  using buffer_type = io_handle::buffer_type;
  using const_buffer_type = io_handle::const_buffer_type;
  using buffers_type = io_handle::buffers_type;
  using const_buffers_type = io_handle::const_buffers_type;
  template <class T> using io_request = io_handle::io_request<T>;
  template <class T> using io_result = io_handle::io_result<T>;

protected:
  section_handle *_section;
  char *_addr;
  extent_type _offset;
  size_type _length;

public:
  //! Default constructor
  map_handle()
      : io_handle()
      , _section(nullptr)
      , _addr(nullptr)
      , _offset(0)
      , _length(0)
  {
  }
  //! Construct from these parameters
  map_handle(io_handle h, section_handle *section)
      : io_handle(std::move(h))
      , _section(section)
      , _addr(nullptr)
      , _offset(0)
      , _length(0)
  {
  }
  ~map_handle();
  //! Implicit move construction of map_handle permitted
  map_handle(map_handle &&o) noexcept : io_handle(std::move(o)), _section(o._section), _addr(o._addr), _offset(o._offset), _length(o._length)
  {
    o._section = nullptr;
    o._addr = nullptr;
    o._offset = 0;
    o._length = 0;
  }
  //! Move assignment of map_handle permitted
  map_handle &operator=(map_handle &&o) noexcept
  {
    this->~map_handle();
    new(this) map_handle(std::move(o));
    return *this;
  }
  //! Swap with another instance
  void swap(map_handle &o) noexcept
  {
    map_handle temp(std::move(*this));
    *this = std::move(o);
    o = std::move(temp);
  }

  //! Unmap the mapped view.
  BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC result<void> close() noexcept;
  //! Releases the mapped view, but does NOT release the native handle.
  BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC native_handle_type release() noexcept;


  /*! Create a memory mapped view of a backing storage.
  \param section A memory section handle specifying the backing storage to use.
  \param bytes How many bytes to map (0 = the size of the memory section). Typically needs to be a multiple of the page size (see utils::page_sizes()).
  \param offset The offset into the backing storage to map from. Typically needs to be at least a multiple of the page size (see utils::page_sizes()), on Windows it needs to be a multiple of the kernel memory allocation granularity (typically 64Kb).
  \param _flag The permissions with which to map the view which are constrained by the permissions of the memory section. `flag::none` can be useful for reserving virtual address space without committing system resources, use commit() to later change availability of memory.

  \errors Any of the values POSIX mmap() or NtMapViewOfSection() can return.
  */
  //[[bindlib::make_free]]
  static BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC result<map_handle> map(section_handle &section, size_type bytes = 0, extent_type offset = 0, section_handle::flag _flag = section_handle::flag::read | section_handle::flag::write) noexcept;

  //! The memory section this handle is using
  section_handle *section() const noexcept { return _section; }

  //! The address in memory where this mapped view resides
  char *address() const noexcept { return _addr; }

  //! The offset of the memory map.
  extent_type offset() const noexcept { return _offset; }

  //! The size of the memory map.
  size_type length() const noexcept { return _length; }

  //! Ask the system to commit the system resources to make the memory represented by the buffer available with the given permissions. addr and length should be page aligned (see utils::page_sizes()), if not the returned buffer is the region actually committed.
  result<buffer_type> commit(buffer_type region, section_handle::flag _flag = section_handle::flag::read | section_handle::flag::write) noexcept;

  //! Ask the system to make the memory represented by the buffer unavailable and to decommit the system resources representing them. addr and length should be page aligned (see utils::page_sizes()), if not the returned buffer is the region actually decommitted.
  result<buffer_type> decommit(buffer_type region) noexcept;

  /*! Zero the memory represented by the buffer. On Linux, Windows and FreeBSD any full 4Kb pages will be deallocated from the system entirely, including the extents for them in any backing storage. On newer Linux kernels the kernel can additionally swap whole 4Kb pages for freshly zeroed ones making this a very
  efficient way of zeroing large ranges of memory.
  */
  result<void> zero(buffer_type region) noexcept;

  //! Ask the system to begin to asynchronously prefetch the span of memory regions given, returning the regions actually prefetched. Note that on Windows 7 or earlier the system call to implement this was not available, and so you will see an empty span returned.
  static result<span<buffer_type>> prefetch(span<buffer_type> regions) noexcept;
  //! \overload
  static result<buffer_type> prefetch(buffer_type region) noexcept { BOOST_OUTCOME_TRY(ret, prefetch(span<buffer_type>(&region, 1))); return *ret.data(); }

  /*! Ask the system to unset the dirty flag for the memory represented by the buffer. This will prevent any changes not yet sent to the backing storage from being sent in the future, also if the system kicks out this page and reloads it you may see some edition of the underlying storage instead of what was here. addr
  and length should be page aligned (see utils::page_sizes()), if not the returned buffer is the region actually undirtied.

  \warning This function destroys the contents of unwritten pages in the region in a totally unpredictable fashion. Only use it if you don't care how much of
  the region reaches physical storage or not. Note that the region is not necessarily zeroed, and may be randomly zeroed.
  */
  static result<buffer_type> do_not_store(buffer_type region) noexcept;

  /*! \brief Read data from the mapped view.

  \note Because this implementation never copies memory, you can pass in buffers with a null address.

  \return The buffers read, which will never be the buffers input because they will point into the mapped view.
  The size of each scatter-gather buffer is updated with the number of bytes of that buffer transferred.
  \param reqs A scatter-gather and offset request.
  \param d Ignored.
  \errors None, though the various signals and structured exception throws common to using memory maps may occur.
  \mallocs None.
  */
  //[[bindlib::make_free]]
  BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC io_result<buffers_type> read(io_request<buffers_type> reqs, deadline d = deadline()) noexcept;
  using io_handle::read;

  /*! \brief Write data to the mapped view.

  \return The buffers written, which will never be the buffers input because they will point at where the data was copied into the mapped view.
  The size of each scatter-gather buffer is updated with the number of bytes of that buffer transferred.
  \param reqs A scatter-gather and offset request.
  \param d Ignored.
  \errors None, though the various signals and structured exception throws common to using memory maps may occur.
  \mallocs None.
  */
  //[[bindlib::make_free]]
  BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC io_result<const_buffers_type> write(io_request<const_buffers_type> reqs, deadline d = deadline()) noexcept;
  using io_handle::write;
};


BOOST_AFIO_V2_NAMESPACE_END

#if BOOST_AFIO_HEADERS_ONLY == 1 && !defined(DOXYGEN_SHOULD_SKIP_THIS)
#define BOOST_AFIO_INCLUDED_BY_HEADER 1
#ifdef _WIN32
#include "detail/impl/windows/map_handle.ipp"
#else
#include "detail/impl/posix/map_handle.ipp"
#endif
#undef BOOST_AFIO_INCLUDED_BY_HEADER
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif