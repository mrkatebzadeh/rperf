/* message.rs

*
* Author: M.R.Siavash Katebzadeh <mr@katebzadeh.xyz>
* Keywords: Rust
* Version: 0.0.1
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/// A struct representing a message with a buffer and a request ID.
pub struct Message {
    buf: Vec<u8>,
    id: u64,
}

impl Message {
    /// Creates a new `Message` with the specified buffer size and request ID.
    ///
    /// # Arguments
    ///
    /// * `size` - The size of the buffer.
    /// * `req_id` - The request identifier.
    #[allow(unused)]
    pub fn new(size: usize, req_id: u64) -> Self {
        Message {
            buf: vec![0; size],
            id: req_id,
        }
    }

    /// Returns the request ID of the message.
    #[allow(unused)]
    pub fn req_id(&self) -> u64 {
        self.id
    }

    /// Returns a reference to the internal buffer of the message.
    #[allow(unused)]
    pub fn buffer(&self) -> &Vec<u8> {
        &self.buf
    }
}

impl From<&[u8]> for Message {
    /// Creates a `Message` from a byte slice.
    fn from(slice: &[u8]) -> Self {
        Message {
            buf: slice.to_vec(),
            id: 0,
        }
    }
}

impl Default for Message {
    /// Creates a default `Message` with an empty buffer and a request ID of 0.
    fn default() -> Self {
        Message::new(0, 0)
    }
}

/* message.rs ends here */
